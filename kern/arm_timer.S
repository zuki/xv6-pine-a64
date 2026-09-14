.global arm_timer_init
arm_timer_init:
  // 1. 仮想タイマーのカウント周波数を取得し、値を設定
  mrs x0, cntfrq_el0
  mov x1, #100
  udiv x0, x0, x1
  msr cntv_tval_el0, x0

  // 2. 仮想タイマーを有効化
  mov x0, #1
  msr cntv_ctl_el0, x0
  ret

.global arm_timer_reset
arm_timer_reset:
  mrs x0, cntfrq_el0
  mov x1, #100
  udiv x0, x0, x1
  msr cntv_tval_el0, x0
  ret

.global get_cntfrq_el0
get_cntfrq_el0:
  mrs x0, cntfrq_el0
  ret
