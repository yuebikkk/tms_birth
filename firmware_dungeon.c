/* =========================================================
 * FIRMWARE DUNGEON  v2.0  ·  ENDLESS DIVE
 * for Tms - 19th birthday - from 悦笔
 * ---------------------------------------------------------
 * 你是烧进 STM32F103C8T6 的一段固件。
 * 一路往下潜，没有底。打得越深，板子越不讲理。
 *
 * A~E 五个按钮是全部操作，屏幕最后一行永远写着它们是什么。
 * 右边 UART 是示波器：每一次伤害的来源都在那里。
 * =======================================================*/

const CRIT_MULT = 1.75;
const ARM_K = 0.85;
const BOSS_EVERY = 4;
const FINALE_FLOOR = 10;

function rnd(n){ return rand(n); }
function pct(p){ return rand(100) < p; }
function clamp(v, a, b){ return v < a ? a : (v > b ? b : v); }
function pad2(n){ return n < 10 ? ('0' + n) : ('' + n); }
function log(s){ uart_print(s); }

function bar(cur, max, n){
  let k = Math.round(clamp(max > 0 ? cur / max : 0, 0, 1) * n);
  let s = '';
  let i;
  for (i = 0; i < n; i++) { s += (i < k ? '#' : '.'); }
  return s;
}

function rarTag(r){
  if (r === 'E') return 'EPC';
  if (r === 'R') return 'RAR';
  if (r === 'X') return '诅咒';
  return 'COM';
}

/* ---------------------------------------------------------
 * 1. 精英词缀：让同一只怪每次都不一样
 * -------------------------------------------------------*/
const AFFIX = {
  HARD:    { id:'HARD',    tag:'坚硬',  arm:3 },
  REGEN:   { id:'REGEN',   tag:'再生',  regen:6 },
  RAGE:    { id:'RAGE',    tag:'狂怒',  rage:40 },
  THORNS:  { id:'THORNS',  tag:'荆棘',  thorns:15 },
  CORRUPT: { id:'CORRUPT', tag:'腐蚀',  corrupt:1 },
  SWIFT:   { id:'SWIFT',   tag:'迅捷',  swift:25 },
  LEECH:   { id:'LEECH',   tag:'吸血',  leech:40 },
  SHELL:   { id:'SHELL',   tag:'护盾',  shell:25 },
  BURN:    { id:'BURN',    tag:'灼热',  burn:1 }
};
const AFFIX_KEYS = ['HARD','REGEN','RAGE','THORNS','CORRUPT','SWIFT','LEECH','SHELL','BURN'];

/* ---------------------------------------------------------
 * 2. 怪物图鉴（18 种）
 * -------------------------------------------------------*/
const MOBS = [
  { id:'NULL_PTR',  hp:22, atk:6,  arm:0, ai:'basic',   minF:1, tag:'每次行动自伤 4',
    name:'NULL_PTR 空指针' },
  { id:'MEM_LEAK',  hp:30, atk:6,  arm:1, ai:'regen',   minF:1, tag:'每回合回血 6%',
    name:'MEM_LEAK 内存泄漏' },
  { id:'INF_LOOP',  hp:26, atk:7,  arm:0, ai:'charge',  minF:1, tag:'每 3 回合重击 2.2x',
    name:'INF_LOOP 死循环' },
  { id:'STACK_OVF', hp:28, atk:7,  arm:2, ai:'pierce',  minF:1, tag:'命中削你护甲',
    name:'STACK_OVF 栈溢出' },
  { id:'RACE_COND', hp:24, atk:6,  arm:1, ai:'double',  minF:2, tag:'30% 概率双动',
    name:'RACE_COND 竞态' },
  { id:'WILD_PTR',  hp:32, atk:5,  arm:1, ai:'chaos',   minF:2, tag:'伤害 0.4x~2.4x',
    name:'WILD_PTR 野指针' },
  { id:'SEGFAULT',  hp:40, atk:10, arm:3, ai:'basic',   minF:2, minElite:0, tag:'高攻高甲',
    name:'SEGFAULT 段错误' },
  { id:'ZOMBIE',    hp:30, atk:7,  arm:0, ai:'split',   minF:2, tag:'死亡分裂一次',
    name:'ZOMBIE_TASK 僵尸任务' },
  { id:'CRC_ERR',   hp:34, atk:8,  arm:1, ai:'corrupt', minF:3, tag:'命中腐蚀暴击',
    name:'CRC_MISMATCH 校验错' },
  { id:'DMA_OVF',   hp:36, atk:8,  arm:2, ai:'stack',   minF:3, tag:'三层溢出后爆伤',
    name:'DMA_OVERFLOW 溢出' },
  { id:'BUS_LOCK',  hp:50, atk:10, arm:6, ai:'thorns',  minF:3, elite:1, tag:'反伤 20%',
    name:'BUS_LOCK 总线锁' },
  { id:'THERMAL',   hp:46, atk:9,  arm:3, ai:'thermal', minF:3, elite:1, tag:'升温后强制降频',
    name:'THERMAL 过温保护' },
  { id:'DANGLING',  hp:32, atk:8,  arm:1, ai:'weaken',  minF:4, tag:'命中削你攻击',
    name:'DANGLING 悬垂指针' },
  { id:'OVERFLOW',  hp:38, atk:8,  arm:2, ai:'desper',  minF:5, tag:'残血后攻击翻倍',
    name:'OVERFLOW 计数溢出' },
  { id:'DEADLOCK',  hp:44, atk:9,  arm:4, ai:'deadlock', minF:5, elite:1, tag:'你防御它就回血',
    name:'DEADLOCK 死锁' },
  { id:'BOF',       hp:36, atk:9,  arm:1, ai:'ignite',  minF:6, tag:'命中附加灼烧',
    name:'BUFFER_OVF 越界写' },
  { id:'INT_STORM', hp:30, atk:5,  arm:0, ai:'storm',   minF:7, tag:'每次行动两连击',
    name:'INT_STORM 中断风暴' },
  { id:'FRAGMENT',  hp:34, atk:9,  arm:2, ai:'boom',    minF:8, tag:'死亡时自爆',
    name:'FRAGMENT 碎片' }
];

/* ---------------------------------------------------------
 * 3. BOSS（6 种，按深度轮转）
 * -------------------------------------------------------*/
const BOSS_ORDER = ['WATCHDOG', 'BOOTLOADER', 'CLOCK_TREE', 'FLASH_CTRL', 'MPU', 'RCC'];

const BOSS_DB = {
  WATCHDOG:   { id:'WATCHDOG',   hp:100, atk:11, arm:3, ai:'watchdog',
                name:'WATCHDOG 看门狗',        tag:'不喂狗就咬人' },
  BOOTLOADER: { id:'BOOTLOADER', hp:138, atk:12, arm:6, ai:'bootloader',
                name:'BOOTLOADER 引导加载器',   tag:'越打越强' },
  CLOCK_TREE: { id:'CLOCK_TREE', hp:186, atk:14, arm:7, ai:'clock',
                name:'CLOCK_TREE 时钟树',      tag:'三阶段狂暴' },
  FLASH_CTRL: { id:'FLASH_CTRL', hp:160, atk:13, arm:5, ai:'flash',
                name:'FLASH_CTRL 闪存控制器',   tag:'擦写会削你的攻击' },
  MPU:        { id:'MPU',        hp:175, atk:12, arm:9, ai:'mpu',
                name:'MPU 内存保护单元',        tag:'隔回合开启保护' },
  RCC:        { id:'RCC',        hp:165, atk:15, arm:5, ai:'rcc',
                name:'RCC 复位时钟控制',        tag:'偷你的能量' }
};

/* ---------------------------------------------------------
 * 4. 技能库（12 个）
 * -------------------------------------------------------*/
const SKILL_DB = [
  { kind:'skill', id:'OVERCLOCK', tag:'超频', rar:'C', cost:3,
    name:'OVERCLOCK 超频', info:'1.9x 伤害，反噬 3' },
  { kind:'skill', id:'PULSE', tag:'脉冲', rar:'C', cost:2,
    name:'PULSE 脉冲', info:'1.15x + 压制 1 回合' },
  { kind:'skill', id:'SHIELD', tag:'稳压', rar:'C', cost:2,
    name:'SHIELD 稳压', info:'护盾 14 + 2x层' },
  { kind:'skill', id:'DRAIN', tag:'抽取', rar:'R', cost:3,
    name:'DRAIN 抽取', info:'1.3x 伤害 + 吸血 55%' },
  { kind:'skill', id:'PHASELOCK', tag:'锁相', rar:'R', cost:2,
    name:'PHASELOCK 锁相环', info:'必定暴击 1.35x' },
  { kind:'skill', id:'ERASE', tag:'擦除', rar:'E', cost:4,
    name:'ERASE 全片擦除', info:'2.7x 伤害，反噬 8' },
  { kind:'skill', id:'FEED', tag:'喂狗', rar:'C', cost:1,
    name:'FEED 喂狗', info:'回 14 HP 并补 1 能量' },
  { kind:'skill', id:'BURN', tag:'过载', rar:'R', cost:2,
    name:'BURN 过载', info:'1.0x + 灼烧 3 回合' },
  { kind:'skill', id:'CHAIN', tag:'链式', rar:'R', cost:3,
    name:'CHAIN 链式响应', info:'0.95x 打两次' },
  { kind:'skill', id:'FREEZE', tag:'冻结', rar:'E', cost:3,
    name:'FREEZE 冻结寄存器', info:'0.8x + 压制 2 回合' },
  { kind:'skill', id:'CURSE', tag:'咒缚', rar:'R', cost:2,
    name:'CURSE 咒缚', info:'敌人攻击永久 -25%' },
  { kind:'skill', id:'SMASH', tag:'压栈', rar:'C', cost:2,
    name:'SMASH 压栈', info:'伤害 = 攻击 + 护甲x3' },
  { kind:'skill', id:'ALLIN', tag:'全押', rar:'E', cost:1,
    name:'ALLIN 全押', info:'一半概率 3.2x，一半自伤 10' }
];

/* ---------------------------------------------------------
 * 5. 道具库（36 件）
 * -------------------------------------------------------*/
const ITEM_DB = [
  /* ---------------- COMMON ---------------- */
  { id:'HEAL_S', rar:'C', kind:'use', tag:'修复包', price:10,
    name:'REPAIR_PACK 修复包', info:'回 30 HP',
    run:function(){ let h=heal(30); log('[USE] 修复包 +'+h+' HP'); } },
  { id:'SPIKE', rar:'C', kind:'use', tag:'电涌', price:12,
    name:'SPIKE 电涌', info:'无视护甲 20 伤害',
    run:function(){ trueDamage(20); } },
  { id:'BOR', rar:'C', kind:'passive', tag:'掉电检测', price:16,
    name:'BROWNOUT 掉电检测', info:'先手免疫一次致死',
    run:function(){ G.bor = (G.bor||0)+1; } },
  { id:'BUF', rar:'C', kind:'passive', tag:'环形缓冲', price:15,
    name:'BUFFER 环形缓冲', info:'maxHP +12 并回满',
    run:function(){ G.maxhp+=12; G.hp+=12; } },
  { id:'PULLUP', rar:'C', kind:'passive', tag:'上拉电阻', price:13,
    name:'PULLUP 上拉电阻', info:'ARM +2',
    run:function(){ G.arm+=2; } },
  { id:'XTAL', rar:'C', kind:'passive', tag:'无源晶振', price:18,
    name:'XTAL 无源晶振', info:'能量上限 +1',
    run:function(){ G.pmax=Math.min(10,G.pmax+1); G.pwr+=1; } },
  { id:'FUSE', rar:'C', kind:'passive', tag:'保险丝', price:14,
    name:'FUSE 保险丝', info:'闪避 +5%',
    run:function(){ G.dge+=5; } },
  { id:'TINY', rar:'C', kind:'passive', tag:'微优化', price:12,
    name:'TINY 编译优化', info:'攻击 +2，暴击 +2%',
    run:function(){ G.atk+=2; G.crit+=2; } },

  /* ---------------- RARE ---------------- */
  { id:'HEAL_L', rar:'R', kind:'use', tag:'全片擦除', price:26,
    name:'FLASH_ERASE 全片擦除', info:'回 65 HP',
    run:function(){ let h=heal(65); log('[USE] 全片擦除 +'+h+' HP'); } },
  { id:'TURBO', rar:'R', kind:'use', tag:'涡轮', price:24,
    name:'TURBO 涡轮', info:'本场 ATK x1.6（3 回合）',
    run:function(){ G.turbo = 3; log('[USE] TURBO 启动'); } },
  { id:'DMA', rar:'R', kind:'passive', tag:'DMA 直通', price:30,
    name:'DMA 直通', info:'攻击 +6',
    run:function(){ G.atk+=6; } },
  { id:'CACHE', rar:'R', kind:'passive', tag:'缓存命中', price:28,
    name:'CACHE 命中', info:'暴击 +9%',
    run:function(){ G.crit+=9; } },
  { id:'NVIC', rar:'R', kind:'passive', tag:'中断嵌套', price:28,
    name:'NVIC 中断嵌套', info:'闪避 +10%',
    run:function(){ G.dge+=10; } },
  { id:'REGEN', rar:'R', kind:'passive', tag:'预分频', price:26,
    name:'IWDG 预分频', info:'每回合回 3 HP',
    run:function(){ G.regen=(G.regen||0)+3; } },
  { id:'BUSM', rar:'R', kind:'passive', tag:'总线矩阵', price:32,
    name:'BUS_MATRIX 总线矩阵', info:'每击杀 ATK +1',
    run:function(){ G.busmatrix=1; } },
  { id:'LEECH', rar:'R', kind:'passive', tag:'电流镜', price:30,
    name:'MIRROR 电流镜', info:'普攻吸血 20%',
    run:function(){ G.leech=(G.leech||0)+20; } },
  { id:'GRILL', rar:'R', kind:'use', tag:'烤机', price:22,
    name:'BURN_IN 烤机', info:'给敌人挂 4 回合灼烧',
    run:function(){ if(!E){ log('[ERR] 不在战斗中'); G.bag.push(SKILL_FIND('GRILL')); return; } E.burn=(E.burn||0)+4; log('[USE] 敌人开始过热'); } },
  { id:'BATTERY', rar:'R', kind:'passive', tag:'备电', price:34,
    name:'BATTERY 备电', info:'能量上限 +2',
    run:function(){ G.pmax=Math.min(10,G.pmax+2); G.pwr+=2; } },

  /* ---------------- EPIC ---------------- */
  { id:'RESETV', rar:'E', kind:'use', tag:'复位向量', price:48,
    name:'RESET_VECTOR 复位向量', info:'清敌人增益 + 25 真伤',
    run:function(){ if(!E){ log('[ERR] 不在战斗中'); G.bag.push(SKILL_FIND('RESETV')); return; }
      E.stack=0; E.temp=0; E.turn=0; E.burn=0; trueDamage(25,'复位'); } },
  { id:'PLL', rar:'E', kind:'use', tag:'锁相环', price:46,
    name:'PLL 锁相环', info:'本回合必暴',
    run:function(){ G.critBoost=1; log('[USE] 本回合必暴'); } },
  { id:'SWD', rar:'E', kind:'passive', tag:'调试口', price:60,
    name:'SWD 调试口', info:'倒下后原地复活一次',
    run:function(){ G.revive=(G.revive||0)+1; } },
  { id:'GOLDEN', rar:'E', kind:'passive', tag:'基准源', price:52,
    name:'GOLDEN_REF 基准源', info:'攻 +4 / 甲 +2 / 暴 +5%',
    run:function(){ G.atk+=4; G.arm+=2; G.crit+=5; } },
  { id:'WDOG', rar:'E', kind:'passive', tag:'喂狗器', price:55,
    name:'FEED_DOG 喂狗器', info:'每 3 回合免一次伤害',
    run:function(){ G.feeddog=1; } },
  { id:'OVERV', rar:'E', kind:'passive', tag:'过压保护', price:58,
    name:'OVERV 过压保护', info:'受到伤害 -12%',
    run:function(){ G.dr=(G.dr||0)+12; } },
  { id:'DUAL', rar:'E', kind:'passive', tag:'双核', price:64,
    name:'DUAL_CORE 双核', info:'普攻有 35% 概率再打一次',
    run:function(){ G.dual=1; } },
  { id:'CRYSTAL2', rar:'E', kind:'passive', tag:'恒温晶振', price:56,
    name:'TCXO 恒温晶振', info:'每回合多回 1 能量',
    run:function(){ G.pwrTick=Math.min(2,(G.pwrTick||1)+1); } },

  /* ---------------- CURSED ---------------- */
  { id:'SMPS', rar:'X', kind:'passive', tag:'SMPS', price:20,
    name:'SMPS 降压 [诅咒]', info:'攻 +11，血上限 -16',
    run:function(){ G.atk+=11; G.maxhp-=16; G.hp=Math.min(G.hp,G.maxhp); } },
  { id:'FLASHW', rar:'X', kind:'passive', tag:'擦写损耗', price:22,
    name:'FLASH_WRITE [诅咒]', info:'每回合自伤 2，攻 +1',
    run:function(){ G.flashw=1; } },
  { id:'HEAPFRAG', rar:'X', kind:'passive', tag:'堆碎片', price:18,
    name:'HEAP_FRAG [诅咒]', info:'血上限 +30，闪避 -12%',
    run:function(){ G.maxhp+=30; G.hp+=30; G.dge=Math.max(0,G.dge-12); } },
  { id:'UNDERCLK', rar:'X', kind:'passive', tag:'降频', price:24,
    name:'UNDERCLOCK [诅咒]', info:'护甲 +5，攻 -3',
    run:function(){ G.arm+=5; G.atk=Math.max(1,G.atk-3); } },
  { id:'BLEEDON', rar:'X', kind:'passive', tag:'漏电', price:20,
    name:'LEAKAGE [诅咒]', info:'每回合掉 3 血，暴击 +15%',
    run:function(){ G.leak=1; G.crit+=15; } },

  /* ---------------- 补给/其他 ---------------- */
  { id:'NUKE', rar:'R', kind:'use', tag:'短路', price:36,
    name:'SHORT 短路', info:'造成敌人当前血量 18% 的伤害',
    run:function(){ if(!E){ log('[ERR] 不在战斗中'); G.bag.push(SKILL_FIND('NUKE')); return; }
      let d=Math.max(6, Math.round(E.hp*0.18)); trueDamage(d,'短路'); } },
  { id:'LIFELINE', rar:'R', kind:'use', tag:'生命线', price:30,
    name:'LIFELINE 生命线', info:'回复 25% 最大生命并加 10 护盾',
    run:function(){ let h=heal(Math.round(G.maxhp*0.25)); addShield(10); log('[USE] 生命线 +'+h+' HP'); } }
];

function SKILL_FIND(id){
  let i;
  for (i = 0; i < ITEM_DB.length; i++) { if (ITEM_DB[i].id === id) return ITEM_DB[i]; }
  return null;
}

/* ---------------------------------------------------------
 * 6. 三种固件人格
 * -------------------------------------------------------*/
const CLASSES = [
  { id:'BOOT', tag:'BOOTLOADER', title:'传承固件',
    desc:'血厚甲高，护甲直接变成伤害',
    hp:100, atk:12, arm:3, crit:5, dge:5, pwr:3, pmax:3, skill:'SMASH',
    hint:'HP100 ARM3' },
  { id:'RTOS', tag:'RTOS 调度', title:'调度固件',
    desc:'能量多、闪避高，技能连发',
    hp:78, atk:11, arm:0, crit:8, dge:8, pwr:4, pmax:4, skill:'PULSE',
    hint:'HP78 PWR4 DGE8' },
  { id:'DSP', tag:'DSP 信号', title:'信号固件',
    desc:'攻击和暴击极高，纸一样薄',
    hp:66, atk:15, arm:0, crit:18, dge:4, pwr:3, pmax:3, skill:'OVERCLOCK',
    hint:'HP66 ATK15 暴18' }
];

function findSkill(id){
  let i;
  for (i = 0; i < SKILL_DB.length; i++) { if (SKILL_DB[i].id === id) return SKILL_DB[i]; }
  return SKILL_DB[0];
}

/* ---------------------------------------------------------
 * 7. 全局状态
 * -------------------------------------------------------*/
let G = null;
let E = null;
let PHASE = 'BOOT';
let PICKS = [];
let SHOP = [];
let EV = null;
let MAPS = [];
let BEST = { floor: 0, kills: 0, dives: 0 };
let beatTimer = null;
let beatState = 0;

function newRun(cls){
  G = {
    floor: 1, hp: cls.hp, maxhp: cls.hp,
    atk: cls.atk, arm: cls.arm, crit: cls.crit, dge: cls.dge,
    pwr: cls.pwr, pmax: cls.pmax, gold: 0, shield: 0,
    skills: [findSkill(cls.skill), null],
    bag: [], kills: 0, turns: 0, cls: cls.tag,
    bor:0, revive:0, regen:0, turbo:0, critBoost:0, busmatrix:0,
    flashw:0, feeddog:0, pwrTick:1, guard:0, leech:0, dr:0, dual:0, leak:0, bleed:0
  };
  G.bag.push(ITEM_DB[0]);
  G.bag.push(ITEM_DB[0]);
  E = null; PICKS = []; SHOP = []; EV = null; MAPS = [];
  PHASE = 'MAP';
}

/* ---------------------------------------------------------
 * 8. 战斗基础运算
 * -------------------------------------------------------*/
/* 护甲：递减型百分比减伤，永远不会被深度抹平 */
function armorMul(arm, k){
  if (arm <= 0) return 1;
  return 100 / (100 + arm * k);
}

function addShield(n){
  G.shield = Math.min(G.shield + n, G.maxhp);
  log('[SHIELD] 护盾 ' + G.shield + '/' + G.maxhp);
}

function heal(n){
  let b = G.hp;
  G.hp = Math.min(G.maxhp, G.hp + n);
  return G.hp - b;
}

function trueDamage(n, tag){
  if (!E) return;
  E.hp -= n;
  log('[' + (tag||'TRUE') + '] 真实伤害 ' + n + '  (HP ' + Math.max(0,E.hp) + '/' + E.maxhp + ')');
}

function pHit(mult, flat, tag, forceCrit){
  if (!E) return 0;
  let raw = G.atk * mult + (flat||0);
  if (G.turbo > 0) raw = raw * 1.6;
  let crit = false;
  if (forceCrit || G.critBoost || pct(clamp(G.crit,0,88))) { raw = raw * CRIT_MULT; crit = true; }
  raw = raw * (0.9 + rnd(21)/100);
  let d = Math.round(raw * armorMul(E.arm, 3));
  if (d < 1) d = 1;

  if (E.sh > 0) {
    let abs = Math.min(E.sh, d);
    E.sh -= abs; d -= abs;
    log('[SHELL] 敌方护盾吸收 ' + abs);
  }
  if (d > 0) E.hp -= d;
  log('[' + (tag||'ATK') + '] ' + (crit?'CRIT ':'') + d + ' -> ' + E.short
      + '  (HP ' + Math.max(0,E.hp) + '/' + E.maxhp + ')');

  if (G.leech > 0 && d > 0) {
    let h = heal(Math.round(d * G.leech / 100));
    if (h > 0) log('[LEECH] 吸回 ' + h);
  }
  if (E.thorns > 0) {
    let th = Math.max(1, Math.round(d * E.thorns / 100));
    G.hp -= th;
    log('[THORNS] 反伤 ' + th + '  (HP ' + Math.max(0,G.hp) + '/' + G.maxhp + ')');
  }
  return d;
}

function eHit(mult, flat){
  if (pct(clamp(G.dge,0,35))) { log('[DODGE] 闪掉了 ' + E.short + ' 的攻击'); return 0; }
  let atk = E.atk;
  if (E.rage > 0 && E.hp * 100 < E.maxhp * 50) atk = Math.round(atk * (1 + E.rage/100));
  let raw = (atk * mult + (flat||0)) * (0.9 + rnd(21)/100);
  if (G.dr > 0) raw = raw * (100 - Math.min(G.dr, 45)) / 100;
  let d = Math.round(raw * armorMul(G.arm, 7));
  if (d < 1) d = 1;

  if (G.feeddog && E.turn % 3 === 0 && pct(60)) { log('[FEED] 喂狗器挡住了一次'); return 0; }
  if (G.shield > 0) {
    let abs = Math.min(G.shield, d);
    G.shield -= abs; d -= abs;
    log('[SHIELD] 吸收 ' + abs);
  }
  if (d > 0) {
    G.hp -= d;
    log('[HIT] ' + E.short + ' -> YOU ' + d + '  (HP ' + Math.max(0,G.hp) + '/' + G.maxhp + ')');
    if (E.leech > 0) {
      let h = Math.round(d * E.leech / 100);
      E.hp = Math.min(E.maxhp, E.hp + h);
      log('[LEECH] 敌方吸回 ' + h);
    }
    if (E.burnAura) { G.bleed = (G.bleed||0) + 2; log('[BURN] 你被引燃 2 回合'); }
  }
  return d;
}

function applyStun(n){
  if (!E) return;
  if (E.boss) { log('[IMMUNE] BOSS 免疫压制'); return; }
  let res = E.stunRes || 0;
  let real = n - res;
  if (real <= 0) {
    E.stunRes = res + 1;
    log('[RESIST] 对方已经适应，压制无效 (抗性 ' + E.stunRes + ')');
    return;
  }
  E.stun = Math.max(E.stun || 0, real);
  E.stunRes = res + 1;
  log('[STUN] 压制 ' + real + ' 回合');
}

function selfDmg(n){
  G.hp -= n;
  log('[RECOIL] 反噬 ' + n + '  (HP ' + Math.max(0,G.hp) + '/' + G.maxhp + ')');
}

/* ---------------------------------------------------------
 * 9. 玩家行动
 * -------------------------------------------------------*/
function castSkill(slot){
  let sk = G.skills[slot];
  if (!sk) { log('[ERR] 技能槽是空的'); return false; }
  if (G.pwr < sk.cost) {
    log('[WARN] 能量不足（' + sk.tag + ' 要 ' + sk.cost + '，你有 ' + G.pwr + '），自动改成普攻');
    pHit(1.0, 0, 'ATK');
    return true;
  }
  G.pwr -= sk.cost;
  log('[SKILL] ' + sk.name);

  if (sk.id === 'OVERCLOCK') { pHit(1.9,0,'超频'); selfDmg(3); }
  else if (sk.id === 'PULSE') { pHit(1.15,0,'脉冲'); applyStun(1); }
  else if (sk.id === 'SHIELD') {
    addShield(14 + G.floor*2);
  }
  else if (sk.id === 'DRAIN') {
    let d = pHit(1.3,0,'抽取'); let h = heal(Math.round(d*0.55)); log('[DRAIN] 吸血 +' + h);
  }
  else if (sk.id === 'PHASELOCK') { pHit(1.35,0,'锁相',true); }
  else if (sk.id === 'ERASE') { pHit(2.7,0,'擦除'); selfDmg(8); }
  else if (sk.id === 'FEED') { let h=heal(14); G.pwr=Math.min(G.pmax,G.pwr+1); log('[FEED] +'+h+' HP, PWR+1'); }
  else if (sk.id === 'BURN') { pHit(1.0,0,'过载'); if(E) E.burn = (E.burn||0)+3; log('[BURN] 敌人过热 3 回合'); }
  else if (sk.id === 'CHAIN') { pHit(0.95,0,'链式'); if(E && E.hp>0) pHit(0.95,0,'链式'); }
  else if (sk.id === 'FREEZE') { pHit(0.8,0,'冻结'); applyStun(2); }
  else if (sk.id === 'CURSE') {
    if (E) { E.atk = Math.max(1, Math.round(E.atk*0.75)); log('[CURSE] 敌人攻击 -25% -> ' + E.atk); }
  }
  else if (sk.id === 'SMASH') { pHit(1.0, G.arm * 3, '压栈'); }
  else if (sk.id === 'ALLIN') {
    if (pct(50)) { log('[ALLIN] 赌中了'); pHit(3.2,0,'全押',true); }
    else { log('[ALLIN] 赌输了'); selfDmg(10); }
  }
  return true;
}

function useItem(){
  if (G.bag.length === 0) { log('[WARN] 背包空，自动改成防御'); doGuard(); return true; }
  let idx = -1;
  let i;
  const order = ['HEAL_S','HEAL_L','LIFELINE'];
  for (i = 0; i < order.length && idx < 0; i++) {
    let j;
    for (j = 0; j < G.bag.length; j++) { if (G.bag[j].id === order[i]) { idx = j; break; } }
  }
  if (idx < 0) idx = 0;
  let it = G.bag[idx];
  G.bag.splice(idx,1);
  log('[ITEM] 使用 ' + it.name);
  it.run();
  return true;
}

function doGuard(){
  G.guard = 1;
  addShield(4);
  G.pwr = Math.min(G.pmax, G.pwr + 1);
  log('[GUARD] 收窄总线：本回合减伤，护盾 +4');
}

/* ---------------------------------------------------------
 * 10. 敌人 AI
 * -------------------------------------------------------*/
function enemyAct(){
  if (!E) return;
  E.turn = (E.turn||0) + 1;
  let ai = E.ai;

  if (ai === 'basic') {
    eHit(1.0);
    if (E.id === 'NULL_PTR') { E.hp -= 4; log('[NULL] 解引用自己，自伤 4'); }
  }
  else if (ai === 'regen') {
    let r = Math.round(E.maxhp*0.06);
    E.hp = Math.min(E.maxhp, E.hp + r);
    log('[LEAK] 泄漏回血 +' + r);
    eHit(0.9);
  }
  else if (ai === 'charge') {
    if (E.turn % 3 === 0) { log('[LOOP] 死循环蓄力完成'); eHit(2.2); }
    else eHit(0.85);
  }
  else if (ai === 'pierce') {
    eHit(1.0);
    if (G.arm > 0) { G.arm -= 1; log('[OVF] 你的护甲 -1 -> ' + G.arm); }
  }
  else if (ai === 'double') {
    eHit(0.8);
    if (pct(30)) { log('[RACE] 重入！第二次攻击'); eHit(0.8); }
  }
  else if (ai === 'chaos') {
    let m = 0.4 + rnd(200)/100;
    log('[PTR] 随机寻址 x' + (Math.round(m*100)/100));
    eHit(m);
  }
  else if (ai === 'corrupt') {
    eHit(1.0);
    if (G.crit > 0) { G.crit = Math.max(0,G.crit-3); log('[CRC] 暴击被腐蚀 -3 -> ' + G.crit + '%'); }
  }
  else if (ai === 'stack') {
    eHit(1.0);
    E.stack = (E.stack||0)+1;
    if (E.stack >= 3) { log('[DMA] 溢出三层，爆炸'); eHit(1.8); E.stack = 0; }
    else log('[DMA] 溢出 ' + E.stack + '/3');
  }
  else if (ai === 'thorns') { eHit(1.0); }
  else if (ai === 'thermal') {
    eHit(1.0);
    E.temp = (E.temp||0)+1;
    log('[TEMP] 温度 ' + E.temp + '/5');
    if (E.temp >= 5) { E.temp = 0; G.pwr = 0; G.turbo = 0; log('[THERMAL] 强制降频：能量清零'); }
  }
  else if (ai === 'weaken') {
    eHit(1.0);
    G.atk = Math.max(1, G.atk - 2);
    log('[DANGLING] 你的攻击 -2 -> ' + G.atk);
  }
  else if (ai === 'desper') {
    if (E.hp * 100 < E.maxhp * 30) { log('[OVF] 计数溢出，攻击翻倍'); eHit(2.0); }
    else eHit(1.0);
  }
  else if (ai === 'deadlock') {
    if (G.guard) { E.hp = Math.min(E.maxhp, E.hp + Math.round(E.maxhp*0.1)); log('[DEADLOCK] 死等，它回血'); }
    eHit(0.95);
  }
  else if (ai === 'ignite') {
    eHit(1.0); G.bleed = (G.bleed||0) + 2;
    log('[BOF] 越界写，你被灼烧 2 回合');
  }
  else if (ai === 'storm') {
    log('[INT] 中断风暴');
    eHit(0.6); eHit(0.6);
  }
  else if (ai === 'boom') { eHit(1.0); }
  else if (ai === 'watchdog') {
    if (E.turn % 3 === 0) { log('[WDT] 喂狗超时，系统复位冲击'); eHit(1.8); }
    else { E.hp = Math.min(E.maxhp, E.hp + 3); eHit(1.0); }
  }
  else if (ai === 'bootloader') {
    if (E.turn === 2) { E.atk += 4; log('[BOOT] 加载完成 ATK +4 -> ' + E.atk); eHit(0.6); }
    else if (E.turn === 4) { E.arm += 3; log('[BOOT] 写保护 ARM +3 -> ' + E.arm); eHit(0.6); }
    else if (E.turn === 7) { E.atk += 5; log('[BOOT] 二次加载 ATK +5 -> ' + E.atk); eHit(0.8); }
    else eHit(1.05);
  }
  else if (ai === 'clock') {
    let r = E.hp / E.maxhp;
    if (r > 0.66) eHit(1.0);
    else if (r > 0.33) { log('[CLK] 倍频 x2，双动'); eHit(0.85); eHit(0.85); }
    else { log('[CLK] 超频狂暴'); eHit(1.45); }
    E.hp = Math.min(E.maxhp, E.hp + 4);
  }
  else if (ai === 'flash') {
    if (E.turn % 2 === 0) {
      eHit(0.7);
      G.atk = Math.max(1, G.atk - 3);
      log('[FLASH] 擦写脉冲，你的攻击 -3 -> ' + G.atk);
    } else eHit(1.1);
  }
  else if (ai === 'mpu') {
    if (E.turn % 2 === 0) { E.guard = 1; log('[MPU] 开启写保护，本回合减伤'); eHit(0.5); }
    else { E.guard = 0; eHit(1.25); }
  }
  else if (ai === 'rcc') {
    if (E.turn % 3 === 0) {
      let stolen = Math.min(G.pwr, 2);
      G.pwr -= stolen;
      log('[RCC] 劫持时钟，偷走 ' + stolen + ' 点能量');
      eHit(0.8);
    } else eHit(1.15);
  }

  /* 词缀回合结算 */
  if (E.regen && E.regen > 0) {
    let r = Math.round(E.maxhp * E.regen / 100);
    E.hp = Math.min(E.maxhp, E.hp + r);
    log('[AFFIX] 再生 +' + r);
  }
  if (E.swift && pct(E.swift) && E.hp > 0) {
    log('[AFFIX] 迅捷追击');
    eHit(0.7);
  }
  if (E.corrupt && pct(40) && G.crit > 0) {
    G.crit = Math.max(0, G.crit - 2);
    log('[AFFIX] 腐蚀：你的暴击 -2 -> ' + G.crit + '%');
  }
}

/* ---------------------------------------------------------
 * 11. 回合结算
 * -------------------------------------------------------*/
function endTurn(){
  G.turns++;
  let tick = G.pwrTick || 1;
  G.pwr = Math.min(G.pmax, G.pwr + tick);

  if (E && E.burn > 0) {
    let d = Math.max(2, Math.round(E.maxhp*0.04));
    E.hp -= d; E.burn--;
    log('[BURN] 敌方过热 -' + d + '  (HP ' + Math.max(0,E.hp) + '/' + E.maxhp + ')');
  }
  if (G.bleed > 0) {
    G.hp -= 3; G.bleed--;
    log('[BLEED] 你被灼烧 -3  (HP ' + Math.max(0,G.hp) + '/' + G.maxhp + ')');
  }
  if (G.regen > 0) {
    let rg = Math.min(G.regen, Math.max(3, Math.round(G.maxhp*0.05)));
    let h = heal(rg);
    if (h>0) log('[REGEN] 自修复 +' + h);
  }
  if (G.leak) { selfDmg(3); }
  if (G.flashw) { selfDmg(2); G.atk += 1; log('[FLASH] 擦写 ATK +1 -> ' + G.atk); }
  if (G.shield > 0) G.shield = Math.floor(G.shield * 0.5);
  if (E && E.stunRes > 0 && E.turn % 3 === 0) E.stunRes--;
  if (G.turbo > 0) G.turbo--;
  G.critBoost = 0;
  G.guard = 0;
}

function playerAct(k){
  if (PHASE !== 'COMBAT' || !E) return;
  if (E.turn >= 45) { log('[WDT] 战斗拖太久，看门狗复位'); return gameOver('看门狗复位'); }

  let ok = true;
  if (k === 'A') {
    pHit(1.0,0,'ATK');
    if (G.dual && pct(35) && E && E.hp > 0) { log('[DUAL] 双核追加一击'); pHit(0.7,0,'ATK'); }
  }
  else if (k === 'B') ok = castSkill(0);
  else if (k === 'C') ok = castSkill(1);
  else if (k === 'D') ok = useItem();
  else if (k === 'E') doGuard();
  else ok = false;

  if (!ok) { renderAll(); return; }
  if (G.hp <= 0) return gameOver('自噬');
  if (E.hp <= 0) return enemyDown();

  if (E.stun > 0) { E.stun--; log('[STUN] ' + E.short + ' 被压制'); }
  else if (G.guard) {
    let save = G.arm; G.arm += 8;
    enemyAct();
    G.arm = save;
  } else enemyAct();

  if (G.hp <= 0) return gameOver('被击穿');
  if (E.hp <= 0) return enemyDown();

  endTurn();
  if (E.hp <= 0) return enemyDown();
  if (G.hp <= 0) return gameOver('自噬');
  renderAll();
}

function enemyDown(){
  if (E.ai === 'split' && !E.split) {
    E.split = 1;
    E.maxhp = Math.round(E.maxhp*0.45);
    E.hp = E.maxhp;
    E.atk = Math.round(E.atk*0.8);
    log('[SPLIT] 僵尸任务分裂出残片 HP ' + E.hp);
    renderAll();
    return;
  }
  if (E.ai === 'boom' && !E.boomed) {
    E.boomed = 1;
    let d = Math.max(3, Math.round(E.atk * 0.6));
    G.hp -= d;
    log('[BOOM] 碎片自爆 ' + d + '  (HP ' + Math.max(0,G.hp) + '/' + G.maxhp + ')');
    if (G.hp <= 0) return gameOver('被碎片炸穿');
  }

  ledsOff();
  G.kills++;
  let gold = 6 + rnd(6) + Math.round(G.floor*1.6);
  if (E.boss) gold = 22 + G.floor*3;
  if (E.elite) gold += 10;
  G.gold += gold;
  log('[KILL] ' + E.name + ' 已擦除  +' + gold + 'G');
  if (G.busmatrix) { G.atk += 1; log('[BUS] 总线矩阵 ATK +1 -> ' + G.atk); }

  let h = heal(Math.round(G.maxhp * 0.16));
  if (h > 0) log('[FIX] 战后自检修复 +' + h + ' HP');

  let wasBoss = E.boss;
  let wasFloor = G.floor;
  E = null;

  if (wasBoss && wasFloor === FINALE_FLOOR) { return finale(); }
  if (wasBoss) { PHASE = 'REWARD'; return openReward(true); }
  PHASE = 'REWARD';
  openReward(false);
}

/* ---------------------------------------------------------
 * 12. 深度曲线
 * -------------------------------------------------------*/
function hpMul(f){ let x = f - 1; return 1 + 0.26*x + 0.022*x*x; }
function atkMul(f){ let x = f - 1; return 1 + 0.11*x + 0.006*x*x; }

function makeEnemy(base, boss){
  let f = G.floor;
  let cyc = Math.max(0, Math.floor(f / BOSS_EVERY) - 1);
  let e = {
    short: base.id, name: base.name, tag: base.tag, ai: base.ai,
    boss: !!boss, elite: !!base.elite, turn:0, stack:0, temp:0,
    stun:0, split:0, burn:0, sh:0, guard:0, boomed:0,
    maxhp: base.hp, hp: base.hp, atk: base.atk, arm: base.arm,
    thorns:0, leech:0, regen:0, rage:0, corrupt:0, swift:0, burnAura:0
  };
  if (boss) {
    e.maxhp = Math.round(base.hp * (1 + 0.33*cyc));
    e.atk = Math.round(base.atk * (1 + 0.19*cyc));
    e.arm = base.arm + cyc*2;
    e.hp = e.maxhp;
    /* BOSS 固定带一个词缀 */
    applyAffix(e, AFFIX_KEYS[rnd(AFFIX_KEYS.length)]);
    if (cyc >= 3) applyAffix(e, AFFIX_KEYS[rnd(AFFIX_KEYS.length)]);
    return e;
  }
  e.maxhp = Math.round(base.hp * hpMul(f));
  e.atk = Math.round(base.atk * atkMul(f));
  e.arm = base.arm + Math.floor((f-1)/4);
  e.hp = e.maxhp;

  /* 精英与深层怪带词缀 */
  let n = 0;
  if (base.elite) n = 1;
  if (f >= 9 && pct(30)) n = 1;
  if (f >= 14) n = Math.max(n, pct(55) ? 2 : 1);
  if (n > 0) {
    let used = [];
    let i;
    for (i = 0; i < n; i++) {
      let k = AFFIX_KEYS[rnd(AFFIX_KEYS.length)];
      let guard = 0;
      while (used.indexOf(k) >= 0 && guard++ < 10) k = AFFIX_KEYS[rnd(AFFIX_KEYS.length)];
      used.push(k);
      applyAffix(e, k);
    }
  }
  return e;
}

function applyAffix(e, key){
  let a = AFFIX[key];
  if (!a) return;
  if (key === 'HARD') e.arm += a.arm;
  else if (key === 'REGEN') e.regen = a.regen;
  else if (key === 'RAGE') e.rage = a.rage;
  else if (key === 'THORNS') e.thorns = a.thorns;
  else if (key === 'CORRUPT') e.corrupt = 1;
  else if (key === 'SWIFT') e.swift = a.swift;
  else if (key === 'LEECH') e.leech = a.leech;
  else if (key === 'SHELL') e.sh = Math.round(e.maxhp * a.shell / 100);
  else if (key === 'BURN') e.burnAura = 1;
  e.affixTag = (e.affixTag ? e.affixTag + '/' : '') + a.tag;
}

/* ---------------------------------------------------------
 * 13. 地图分支
 * -------------------------------------------------------*/
function openMap(){
  PHASE = 'MAP';
  let next = G.floor;
  if (next % BOSS_EVERY === 0) { return startBoss(); }
  let pool = ['fight','fight','event','shop'];
  if (next >= 3) pool.push('elite');
  if (next >= 6) pool.push('elite');
  let picks = [];
  let guard = 0;
  while (picks.length < 3 && guard++ < 40) {
    let k = pool[rnd(pool.length)];
    if (picks.indexOf(k) >= 0) continue;
    picks.push(k);
  }
  MAPS = picks;
  renderAll();
}

const NODE_NAME = { fight:'战斗', elite:'精英', event:'事件', shop:'商店' };
const NODE_DESC = {
  fight:'普通敌人，稳',
  elite:'强敌，掉落更好',
  event:'随机事件，可能很赚',
  shop:'花钱买装备'
};

function nodeAct(k){
  if (k === 'E') { showStatus(); renderAll(); return; }
  let idx = k === 'A' ? 0 : (k === 'B' ? 1 : (k === 'C' ? 2 : -1));
  if (idx < 0 || !MAPS[idx]) { log('[ERR] 这个位置是空的'); return; }
  let kind = MAPS[idx];
  MAPS = [];
  log('[PATH] 你选择了：' + NODE_NAME[kind]);
  if (kind === 'fight') startCombat(false);
  else if (kind === 'elite') startCombat(true);
  else if (kind === 'event') startEvent();
  else startShop();
}

/* ---------------------------------------------------------
 * 14. 战斗入口
 * -------------------------------------------------------*/
function startCombat(forceElite){
  let pool = [];
  let i;
  for (i = 0; i < MOBS.length; i++) {
    let m = MOBS[i];
    if (m.minF > G.floor) continue;
    if (forceElite) {
      if (m.elite || G.floor >= 6) pool.push(m);
    } else {
      pool.push(m);
    }
  }
  if (pool.length === 0) pool = [MOBS[0]];
  let base = pool[rnd(pool.length)];
  E = makeEnemy(base, false);
  if (forceElite) {
    E.elite = 1;
    E.maxhp = Math.round(E.maxhp * 1.3);
    E.hp = E.maxhp;
    E.atk = Math.round(E.atk * 1.2);
    if (!E.affixTag) applyAffix(E, AFFIX_KEYS[rnd(AFFIX_KEYS.length)]);
  }
  PHASE = 'COMBAT';
  log('');
  log('--- F' + pad2(G.floor) + ' 遭遇 ---');
  log('[SPAWN] ' + E.name + (E.elite?' [精英]':''));
  log('        ' + E.tag + '  |  HP ' + E.maxhp + ' ATK ' + E.atk + ' ARM ' + E.arm);
  if (E.affixTag) log('        词缀: ' + E.affixTag);
  log('[ME]    HP ' + G.hp + '/' + G.maxhp + ' ATK ' + G.atk + ' ARM ' + G.arm);
  log('[BAG]   ' + bagSummary());
  renderAll();
}

function startBoss(){
  let cyc = Math.max(1, Math.floor(G.floor / BOSS_EVERY));
  let id = BOSS_ORDER[(cyc - 1) % BOSS_ORDER.length];
  E = makeEnemy(BOSS_DB[id], true);
  if (cyc > BOSS_ORDER.length) E.short = E.short + '+';
  PHASE = 'COMBAT';
  log('');
  log('--- F' + pad2(G.floor) + ' BOSS ---');
  log('[BOSS] ' + E.name + (cyc > BOSS_ORDER.length ? (' MK' + (Math.floor((cyc-1)/BOSS_ORDER.length)+1)) : ''));
  log('        ' + E.tag + '  |  HP ' + E.maxhp + ' ATK ' + E.atk + ' ARM ' + E.arm);
  if (E.affixTag) log('        词缀: ' + E.affixTag);
  renderAll();
}

/* ---------------------------------------------------------
 * 15. 奖励
 * -------------------------------------------------------*/
function rollItem(floor){
  if (pct(22)) {
    let pool = [];
    let i;
    for (i = 0; i < SKILL_DB.length; i++) {
      let sk = SKILL_DB[i];
      let owned = (G.skills[0] && G.skills[0].id === sk.id) || (G.skills[1] && G.skills[1].id === sk.id);
      if (!owned) pool.push(sk);
    }
    if (pool.length > 0) return pool[rnd(pool.length)];
  }
  let r = rnd(100) + floor*2.2;
  let rar = 'C';
  if (r > 96) rar = 'E';
  else if (r > 66) rar = 'R';
  else if (r < 10) rar = 'X';
  let pool = [];
  let i;
  for (i = 0; i < ITEM_DB.length; i++) { if (ITEM_DB[i].rar === rar) pool.push(ITEM_DB[i]); }
  if (pool.length === 0) pool = [ITEM_DB[0]];
  return pool[rnd(pool.length)];
}

function openReward(bossBonus){
  PICKS = [];
  let n = bossBonus ? 4 : 3;
  if (n > 3) n = 3;
  let guard = 0;
  while (PICKS.length < n && guard++ < 80) {
    let it = rollItem(G.floor);
    let dup = false;
    let i;
    for (i = 0; i < PICKS.length; i++) { if (PICKS[i].id === it.id) dup = true; }
    if (dup) continue;
    PICKS.push(it);
  }
  G.rerolled = 0;
  if (bossBonus) { G.gold += 10; log('[BOSS] 额外赏金 +10G'); }
  log('[REWARD] 三选一（详情见下）');
  let i;
  for (i = 0; i < PICKS.length; i++) {
    let p = PICKS[i];
    let kind = p.kind === 'skill' ? '技能' : (p.kind === 'use' ? '消耗' : '被动');
    log('  ' + 'ABCD'[i] + ' [' + rarTag(p.rar) + '/' + kind + '] ' + p.name + ' —— ' + p.info);
  }
  renderAll();
}

function acquire(it){
  if (it.kind === 'skill') {
    if (!G.skills[0]) G.skills[0] = it;
    else if (!G.skills[1]) G.skills[1] = it;
    else { log('[SWAP] ' + G.skills[1].tag + ' 被 ' + it.tag + ' 替换'); G.skills[1] = it; }
    log('[GET] 技能 ' + it.name + ' —— ' + it.info);
  } else if (it.kind === 'use') {
    G.bag.push(it);
    log('[GET] 道具 ' + it.tag + ' (' + it.info + ')  背包 ' + G.bag.length);
  } else {
    it.run();
    log('[GET] 被动 ' + it.name + ' —— ' + it.info);
    checkSynergy();
  }
}

function checkSynergy(){
  let ids = {};
  let i;
  for (i = 0; i < G.bag.length; i++) ids[G.bag[i].id] = 1;
  if (G.atkBonusDone) return;
  if (G.busmatrix && G.dual && !G.syn1) { G.syn1 = 1; G.atk += 5; log('[SYNERGY] 总线矩阵 + 双核 —— 攻 +5'); }
  if (G.pmax >= 8 && !G.syn2) { G.syn2 = 1; G.pwrTick = 2; log('[SYNERGY] 能量上限爆发 —— 每回合回能 +1'); }
  if (G.arm >= 12 && !G.syn3) { G.syn3 = 1; G.dr = (G.dr||0) + 10; log('[SYNERGY] 高阻态 —— 受到伤害 -10%'); }
}

function pickReward(k){
  if (k === 'A' || k === 'B' || k === 'C') {
    let idx = k === 'A' ? 0 : (k === 'B' ? 1 : 2);
    let it = PICKS[idx];
    if (!it) { log('[ERR] 这一项不存在'); return; }
    acquire(it);
    PICKS = [];
    nextFloor();
  } else if (k === 'D') {
    if (G.rerolled) { log('[ERR] 已经重掷过了'); return; }
    if (G.gold < 15) { log('[ERR] 重掷需要 15G，你只有 ' + G.gold + 'G'); return; }
    G.gold -= 15; G.rerolled = 1;
    log('[REROLL] 花 15G 重掷');
    PICKS = [];
    openReward(false);
    G.rerolled = 1;
  } else if (k === 'E') {
    let g = 12 + G.floor*2;
    G.gold += g;
    log('[SKIP] 放弃奖励 +' + g + 'G  (共 ' + G.gold + 'G)');
    PICKS = [];
    nextFloor();
  }
}

/* ---------------------------------------------------------
 * 16. 商店
 * -------------------------------------------------------*/
function startShop(){
  PHASE = 'SHOP';
  SHOP = [];
  let guard = 0;
  while (SHOP.length < 3 && guard++ < 60) {
    let it = rollItem(G.floor);
    let dup = false;
    let i;
    for (i = 0; i < SHOP.length; i++) { if (SHOP[i].id === it.id) dup = true; }
    if (dup) continue;
    SHOP.push(it);
  }
  log('');
  log('--- SHOP (你有 ' + G.gold + 'G) ---');
  let i;
  for (i = 0; i < SHOP.length; i++) {
    log('  ' + 'ABCD'[i] + ' ' + SHOP[i].name + ' [' + rarTag(SHOP[i].rar) + '] '
        + shopPrice(SHOP[i]) + 'G —— ' + SHOP[i].info);
  }
  log('  D 修复自己 25G（回 50% HP）   E 离开');
  renderAll();
}

function shopPrice(it){
  return Math.round((it.price || 20) * (1 + G.floor * 0.1));
}

function shopAct(k){
  if (k === 'A' || k === 'B' || k === 'C') {
    let idx = k === 'A' ? 0 : (k === 'B' ? 1 : 2);
    let it = SHOP[idx];
    if (!it) { log('[ERR] 货架空了'); return; }
    let p = shopPrice(it);
    if (G.gold < p) { log('[ERR] 金币不够：' + p + 'G，你只有 ' + G.gold + 'G'); return; }
    G.gold -= p;
    SHOP.splice(idx, 1);
    acquire(it);
    log('[BUY] -' + p + 'G，剩 ' + G.gold + 'G');
    renderAll();
  } else if (k === 'D') {
    if (G.gold < 25) { log('[ERR] 修复要 25G'); return; }
    G.gold -= 25;
    let h = heal(Math.round(G.maxhp*0.5));
    log('[BUY] 修复 +' + h + ' HP，剩 ' + G.gold + 'G');
    renderAll();
  } else if (k === 'E') {
    log('[SHOP] 离开');
    nextFloor();
  }
}

/* ---------------------------------------------------------
 * 17. 事件
 * -------------------------------------------------------*/
function rollRarity(rar){
  if (rar === 'S') return SKILL_DB[rnd(SKILL_DB.length)];
  let pool = [];
  let i;
  for (i = 0; i < ITEM_DB.length; i++) { if (ITEM_DB[i].rar === rar) pool.push(ITEM_DB[i]); }
  if (pool.length === 0) pool = [ITEM_DB[0]];
  return pool[rnd(pool.length)];
}

function startEvent(){
  PHASE = 'EVENT';
  let roll = rnd(6);
  if (roll === 0) {
    EV = { title:'熔断器过载', hlp:'牺牲 20% 生命换 EPC', opts:[
      { label:'献祭', run:function(){
          let d = Math.round(G.maxhp*0.2);
          G.maxhp -= d; G.hp = Math.min(G.hp, G.maxhp);
          log('[FUSE] 熔断 ' + d + ' 点上限');
          acquire(rollRarity('E'));
        } },
      { label:'离开', run:function(){ log('[EVENT] 你退出了'); } }
    ]};
  } else if (roll === 1) {
    EV = { title:'随机数发生器', hlp:'25G 赌 60/25/15', opts:[
      { label:'投币', run:function(){
          if (G.gold < 25) { log('[ERR] 金币不足'); return; }
          G.gold -= 25;
          let r = rnd(100);
          if (r < 60) { log('[RNG] RARE'); acquire(rollRarity('R')); }
          else if (r < 85) { log('[RNG] EPIC！'); acquire(rollRarity('E')); }
          else log('[RNG] 空转');
        } },
      { label:'离开', run:function(){ log('[EVENT] 你走了'); } }
    ]};
  } else if (roll === 2) {
    EV = { title:'修复工位', hlp:'免费回 45% 生命', opts:[
      { label:'修复', run:function(){ let h=heal(Math.round(G.maxhp*0.45)); log('[REPAIR] +' + h + ' HP'); } },
      { label:'离开', run:function(){ log('[EVENT] 你走了'); } }
    ]};
  } else if (roll === 3) {
    EV = { title:'参数校准台', hlp:'选一项永久强化', opts:[
      { label:'攻 +4', run:function(){ G.atk+=4; log('[CAL] 攻 +4 -> ' + G.atk); } },
      { label:'甲 +3', run:function(){ G.arm+=3; log('[CAL] 甲 +3 -> ' + G.arm); } },
      { label:'暴 +7%', run:function(){ G.crit+=7; log('[CAL] 暴 +7 -> ' + G.crit + '%'); } }
    ]};
  } else if (roll === 4) {
    EV = { title:'回收站', hlp:'卖光消耗品换金币', opts:[
      { label:'全部卖掉', run:function(){
          let g = 0;
          while (G.bag.length > 0) { let it = G.bag.pop(); g += 12 + (it.price||10)/2; }
          g = Math.round(g);
          G.gold += g;
          log('[SELL] 回收 +' + g + 'G  (共 ' + G.gold + 'G)');
        } },
      { label:'离开', run:function(){ log('[EVENT] 你走了'); } }
    ]};
  } else {
    EV = { title:'深潜井', hlp:'掉 15% 生命跳过一层', opts:[
      { label:'跳下去', run:function(){
          let d = Math.round(G.hp*0.15);
          G.hp -= d;
          log('[RIFT] 你掉了 ' + d + ' 点血，直接穿过一层');
          G.floor++;
          G.atk += 2; G.maxhp += 5; G.hp += 5;
          log('[LEVEL] F' + pad2(G.floor));
          openMap();
        } },
      { label:'不跳', run:function(){ log('[EVENT] 你绕开了井'); } }
    ]};
  }
  log('');
  log('--- EVENT: ' + EV.title + ' ---');
  log('    ' + EV.hlp);
  renderAll();
}

function eventAct(k){
  if (k === 'E') { showStatus(); renderAll(); return; }
  let idx = k === 'A' ? 0 : (k === 'B' ? 1 : (k === 'C' ? 2 : (k === 'D' ? 3 : -1)));
  if (idx < 0 || !EV || !EV.opts[idx]) { log('[ERR] 没有这个选项'); return; }
  let fn = EV.opts[idx].run;
  let jumped = false;
  log('[EVENT] ' + EV.opts[idx].label);
  EV = null;
  fn();
  if (PHASE === 'MAP' || PHASE === 'COMBAT' || PHASE === 'SHOP') return;
  nextFloor();
}

/* ---------------------------------------------------------
 * 18. 层推进
 * -------------------------------------------------------*/
function nextFloor(){
  G.floor++;
  G.atk += 3;
  G.maxhp += 6;
  G.hp += 6;
  if (G.floor % 3 === 1) { G.pmax = Math.min(10, G.pmax + 1); G.pwr += 1; }
  if (G.floor > BEST.floor) BEST.floor = G.floor;
  log('[DEPTH] 下潜到 F' + pad2(G.floor) + '  (ATK ' + G.atk + ' / HP ' + G.hp + '/' + G.maxhp + ')');
  openMap();
}

/* ---------------------------------------------------------
 * 19. 终局
 * -------------------------------------------------------*/
function finale(){
  PHASE = 'FINALE';
  log('');
  log('[CLK] 时钟树安静了。整块板子的时序现在归你。');
  log('[CLK] 你可以停在这里，也可以继续往下 —— 下面没有底。');
  renderAll();
}

function finaleAct(k){
  if (k === 'A') { return victory(); }
  if (k === 'B') {
    log('[DIVE] 你选择继续深潜');
    nextFloor();
    return;
  }
  if (k === 'E') { showStatus(); renderAll(); }
}

function gameOver(reason){
  if (G.revive > 0) {
    G.revive--;
    G.hp = Math.round(G.maxhp * 0.5);
    G.shield = 0;
    log('[SWD] 调试口接管 —— 你被重新挂起，回 50% HP');
    renderAll();
    return;
  }
  if (G.bor > 0) {
    G.bor--;
    G.hp = Math.round(G.maxhp * 0.35);
    log('[BOR] 掉电检测拦住了一次死亡，回 35% HP');
    renderAll();
    return;
  }
  PHASE = 'DEAD';
  ledsOff();
  BEST.dives++;
  if (G.kills > BEST.kills) BEST.kills = G.kills;
  log('');
  log('[FATAL] ' + reason + ' —— 进程在 F' + G.floor + ' 被终止');
  log('[STAT] 击杀 ' + G.kills + ' / 回合 ' + G.turns + ' / 金币 ' + G.gold);
  log('[BEST] 最深 F' + BEST.floor + ' / 最多击杀 ' + BEST.kills);
  log('按 A 换个人格再来');
  renderAll();
}

function victory(){
  PHASE = 'WIN';
  BEST.dives++;
  log('');
  log('[OK] 你在 F' + pad2(G.floor) + ' 封神。这块板子再也不会重启了。');
  log('[OK] 19 岁生日快乐，Tms。');
  log('[19] 19 下闪光，一年一下');
  let i;
  for (i = 0; i < 19; i++) {
    led_on(0); led_on(1); led_on(2); led_on(3);
    delay(55);
    led_off(0); led_off(1); led_off(2); led_off(3);
    delay(75);
  }
  renderAll();
}

/* ---------------------------------------------------------
 * 20. 状态与渲染
 * -------------------------------------------------------*/
function bagSummary(){
  if (G.bag.length === 0) return '空';
  let s = '';
  let i;
  for (i = 0; i < G.bag.length; i++) s += G.bag[i].tag + ' ';
  return s + '(' + G.bag.length + ')';
}

function showStatus(){
  log('=== STATUS F' + pad2(G.floor) + ' / ' + G.cls + ' ===');
  log('HP ' + G.hp + '/' + G.maxhp + '  SH ' + G.shield + '  ATK ' + G.atk
      + '  ARM ' + G.arm + '  CRIT ' + G.crit + '%  DGE ' + G.dge + '%');
  log('PWR ' + G.pwr + '/' + G.pmax + '  回复 ' + (G.pwrTick||1) + '/回合  GOLD ' + G.gold
      + 'G  KILL ' + G.kills + '  TURN ' + G.turns);
  log('技能: ' + (G.skills[0] ? G.skills[0].tag : '-') + ' / '
      + (G.skills[1] ? G.skills[1].tag : '-'));
  log('背包: ' + bagSummary());
}

function tagOf(slot){
  let sk = G.skills[slot];
  return sk ? sk.tag : '--';
}

function renderAll(){
  if (PHASE === 'PICK') {
    oled_show('选一个固件人格\nA ' + CLASSES[0].hint + '\nB ' + CLASSES[1].hint
      + '\nC ' + CLASSES[2].hint + '\nD/E 看说明');
    ledsOff();
    return;
  }
  if (PHASE === 'COMBAT' && E) {
    let s = '';
    s += 'F' + pad2(G.floor) + ' ' + (E.boss ? 'BOSS ' : (E.elite ? 'ELITE ' : '')) + E.short + '\n';
    s += 'HP[' + bar(E.hp, E.maxhp, 8) + ']' + Math.max(0,E.hp) + '/' + E.maxhp + '\n';
    s += 'ME ' + Math.max(0,G.hp) + '/' + G.maxhp + ' +' + G.shield + ' P' + G.pwr + '/' + G.pmax + '\n';
    s += 'ATK' + G.atk + ' ARM' + G.arm + ' C' + G.crit + ' D' + G.dge + '\n';
    s += 'A打 B' + tagOf(0) + ' C' + tagOf(1) + ' D药' + G.bag.length + ' E守';
    oled_show(s);
    ledsEnemy();
    return;
  }
  if (PHASE === 'MAP') {
    let s = 'F' + pad2(G.floor) + ' 选择路径\n';
    let i;
    for (i = 0; i < MAPS.length; i++) {
      s += 'ABC'[i] + ' ' + NODE_NAME[MAPS[i]] + '\n';
    }
    if (MAPS.length < 3) s += '\n';
    s += 'E 看状态';
    oled_show(s);
    ledsOff();
    return;
  }
  if (PHASE === 'REWARD') {
    let s = 'F' + pad2(G.floor) + ' CLEAR 选一个\n';
    let i;
    for (i = 0; i < PICKS.length; i++) {
      s += 'ABC'[i] + ' ' + rarTag(PICKS[i].rar) + ' ' + PICKS[i].tag + '\n';
    }
    s += 'D重掷15G E跳过+' + (12 + G.floor*2) + 'G';
    oled_show(s);
    ledsOff();
    return;
  }
  if (PHASE === 'SHOP') {
    let s = 'SHOP ' + G.gold + 'G\n';
    let i;
    for (i = 0; i < SHOP.length; i++) {
      s += 'ABC'[i] + ' ' + SHOP[i].tag + ' ' + shopPrice(SHOP[i]) + 'G\n';
    }
    s += 'D 修复 25G\nE 离开';
    oled_show(s);
    ledsOff();
    return;
  }
  if (PHASE === 'EVENT' && EV) {
    let s = 'EVENT ' + EV.title + '\n' + EV.hlp + '\n';
    let i;
    for (i = 0; i < EV.opts.length && i < 4; i++) {
      s += 'ABCD'[i] + ' ' + EV.opts[i].label + '  ';
    }
    oled_show(s);
    ledsOff();
    return;
  }
  if (PHASE === 'FINALE') {
    oled_show('时钟树归你了\nA 停在这里（结局）\nB 继续往下潜\nE 看状态');
    ledsOff();
    return;
  }
  if (PHASE === 'DEAD') {
    oled_show('DEAD F' + pad2(G.floor) + ' 击杀 ' + G.kills + '\n最深 F'
      + BEST.floor + ' 局数 ' + BEST.dives + '\nA 再来一局');
    ledsOff();
    return;
  }
  if (PHASE === 'WIN') {
    oled_show('OVERCLOCKED\nF' + pad2(G.floor) + ' 封神\nA 再来一局');
    return;
  }
}

function ledsOff(){
  let i;
  for (i = 0; i < 4; i++) led_off(i);
}

function ledsEnemy(){
  if (!E) { ledsOff(); return; }
  let r = E.maxhp > 0 ? E.hp / E.maxhp : 0;
  let i;
  for (i = 0; i < 4; i++) {
    if (r > i*0.25) led_on(i); else led_off(i);
  }
}

function startBeat(){
  if (beatTimer) return;
  beatTimer = set_interval(200, function(){
    if (PHASE !== 'COMBAT' || !E) return;
    if (G.hp * 100 > G.maxhp * 30) return;
    beatState = 1 - beatState;
    let i;
    for (i = 0; i < 4; i++) {
      if (beatState) led_on(i); else led_off(i);
    }
  });
}

/* ---------------------------------------------------------
 * 21. 输入分发
 * -------------------------------------------------------*/
function onInput(k){
  if (PHASE === 'PICK') {
    if (k === 'A') { startRun(CLASSES[0]); }
    else if (k === 'B') { startRun(CLASSES[1]); }
    else if (k === 'C') { startRun(CLASSES[2]); }
    else if (k === 'D' || k === 'E') {
      let i;
      for (i = 0; i < 3; i++) {
        log('[' + CLASSES[i].tag + '] ' + CLASSES[i].title + ' —— ' + CLASSES[i].desc
            + '  (' + CLASSES[i].hint + ')');
      }
    }
    return;
  }
  if (PHASE === 'COMBAT') return playerAct(k);
  if (PHASE === 'MAP') return nodeAct(k);
  if (PHASE === 'REWARD') return pickReward(k);
  if (PHASE === 'SHOP') return shopAct(k);
  if (PHASE === 'EVENT') return eventAct(k);
  if (PHASE === 'FINALE') return finaleAct(k);
  if (PHASE === 'DEAD' || PHASE === 'WIN') {
    if (k === 'A') { PHASE = 'PICK'; renderAll(); log('[SYS] 选一个固件人格'); }
    else if (k === 'B' || k === 'E') showStatus();
    return;
  }
  log('[ERR] 当前没有可执行的操作');
}

function startRun(cls){
  newRun(cls);
  log('');
  log('=== ' + cls.tag + ' 上线 | ' + cls.title + ' ===');
  log('    ' + cls.desc);
  log('[ME] HP ' + G.hp + ' ATK ' + G.atk + ' ARM ' + G.arm
      + ' CRIT ' + G.crit + '% 技能 ' + G.skills[0].tag);
  openMap();
}

/* ---------------------------------------------------------
 * 22. 开机
 * -------------------------------------------------------*/
function bootFirmware(){
  oled_show('FIRMWARE DUNGEON\nv2.0  ENDLESS DIVE');
  log('');
  log('==============================================');
  log(' FIRMWARE DUNGEON  v2.0  ·  ENDLESS DIVE');
  log(' for Tms - 19th - from 悦笔');
  log('==============================================');
  log('你被烧进这块板子，往下没有底。');
  log('战斗: A打 B技能1 C技能2 D道具 E防御');
  log('其他: A~C 做选择，D/E 有别的用处，看屏幕');
  log('[SYS] 目标：活过 F10 的时钟树，然后看你能潜多深');

  let i;
  for (i = 0; i < 4; i++) { led_on(i); delay(60); }
  for (i = 3; i >= 0; i--) { led_off(i); delay(60); }

  button_add('A', function(){ onInput('A'); });
  button_add('B', function(){ onInput('B'); });
  button_add('C', function(){ onInput('C'); });
  button_add('D', function(){ onInput('D'); });
  button_add('E', function(){ onInput('E'); });

  startBeat();
  PHASE = 'PICK';
  renderAll();
  log('');
  log('[PICK] 选一个固件人格：A ' + CLASSES[0].tag + ' / B ' + CLASSES[1].tag
      + ' / C ' + CLASSES[2].tag + '，D 看说明');
}

bootFirmware();
