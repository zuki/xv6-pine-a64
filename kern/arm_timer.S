.global arm_vtimer_init
// 仮想タイマーの初期化
arm_vtimer_init:
  // 1. 10msで発火するよう仮想タイマーを設定
  //    240,000 = 24MHz / 100
  ldr x0, =240000
  msr cntv_tval_el0, x0

  // 2. 仮想タイマーを有効化
  mov x0, #1
  msr cntv_ctl_el0, x0
  ret

.global arm_vtimer_reset
// 仮想タイマーのリセット
arm_vtimer_reset:
  ldr x0, =240000
  msr cntv_tval_el0, x0
  ret

// void arm_ptimer_init(int interval);
.global arm_ptimer_init
// 物理タイマーの初期化
arm_ptimer_init:
  // 1. 指定の間隔を物理タイマーに設定
  msr cntp_tval_el0, x0

  // 2. 仮想タイマーを有効化
  mov x0, #1
  msr cntp_ctl_el0, x0
  ret

// oid arm_ptimer_reset(int interval)
.global arm_ptimer_reset
// 物理タイマーのリセット
arm_ptimer_reset:
  msr cntp_tval_el0, x0
  ret

.global get_cntfrq_el0
get_cntfrq_el0:
  mrs x0, cntfrq_el0
  ret
