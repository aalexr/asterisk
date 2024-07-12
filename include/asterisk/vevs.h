#pragma once

struct maybe_set {
  int set;
  int val;
};
typedef struct maybe_set maybe_set;

struct maybe_set_twice {
  struct maybe_set a;
  struct maybe_set b;
};
typedef struct maybe_set_twice maybe_set_twice;

struct evs_attr {
  struct maybe_set ftmp_evs_mode_switch;
  struct maybe_set ftmp_hf_only;
  struct maybe_set ftmp_dtx;
  struct maybe_set ftmp_dtx_recv;
  struct maybe_set ftmp_channels;
  struct maybe_set ftmp_cmr;
  struct maybe_set ftmp_ch_send;
  struct maybe_set ftmp_ch_recv;
  struct maybe_set ftmp_ch_aw_recv;
  struct maybe_set ftmp_max_red;
  struct maybe_set_twice ftmp_range_br;
  struct maybe_set_twice ftmp_range_br_send;
  struct maybe_set_twice ftmp_range_br_recv;
  struct maybe_set_twice ftmp_range_bw;
  struct maybe_set_twice ftmp_range_bw_send;
  struct maybe_set_twice ftmp_range_bw_recv;
};
typedef struct evs_attr evs_attr;
