#define FSM_ERROR (-1)

struct fsm_rule {
  int state;
  int (*pred)(int in);
  int in;
  int next;
  int (*fn)(int in, void *context);
};

struct fsm_state {
  int nrules;
  struct fsm_rule *rules;
};

struct fsm {
  int nstates;
  struct fsm_state *states;
};

struct fsm *make_fsm(struct fsm_rule *rules);
int fsm_run_rules(struct fsm_rule *rules, int in, int state, void *context);
int fsm_run(struct fsm *fsm, int in, int state, void *context);

void print_fsm(struct fsm *fsm);

int fsm_char(int in);
int fsm_true(int in);
int fsm_reject(int in, void *context);
