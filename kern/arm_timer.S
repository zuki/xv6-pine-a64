.global arm_vtimer_init
// 仮想タイマーの初期化
arm_vtimer_init:
  // 1. カウント周波数を取得し、仮想タイマーの値を設定
  mrs x0, cntfrq_el0
  mov x1, #100
  udiv x0, x0, x1
  msr cntv_tval_el0, x0

  // 2. 仮想タイマーを有効化
  mov x0, #1
  msr cntv_ctl_el0, x0
  ret

.global arm_vtimer_reset
// 仮想タイマーのリセット
arm_vtimer_reset:
  mrs x0, cntfrq_el0
  mov x1, #100
  udiv x0, x0, x1
  msr cntv_tval_el0, x0
  ret

.global arm_ptimer_init
// 物理タイマーの初期化
arm_ptimer_init:
  // 1. カウント周波数を取得し、物理タイマーの値を設定
  mrs x0, cntfrq_el0
  mov x1, #100
  udiv x0, x0, x1
  msr cntp_tval_el0, x0

  // 2. 仮想タイマーを有効化
  mov x0, #1
  msr cntp_ctl_el0, x0
  ret

.global arm_ptimer_reset
// 物理タイマーのリセット
arm_ptimer_reset:
  mrs x0, cntfrq_el0
  mov x1, #100
  udiv x0, x0, x1
  msr cntp_tval_el0, x0
  ret

.global get_cntfrq_el0
get_cntfrq_el0:
  mrs x0, cntfrq_el0
  ret
