#define FSM_ERROR (-1)

struct fsm_rule {
  int state; /* state where this rule belongs to (used in make_fsm()) */
  int (*pred)(int in);
  int in; /* data for special predicate fsm_char(), matches to given value */
  int next; /* new state */
  int (*fn)(int in, void *context); /* is called when rule matches */
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
int fsm_run(struct fsm *fsm, int in, int state, void *context);
void print_fsm(struct fsm *fsm);
int fsm_char(int in);
int fsm_true(int in);
int fsm_reject(int in, void *context);
