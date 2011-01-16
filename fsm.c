#include <stdlib.h>
#include "fsm.h"

int
fsm_run(struct fsm *fsm, int in, int state, void *context)
{
  int i, n, r;
  struct fsm_rule *pr;

  if (state < 0 || state >= fsm->nstates)
    return FSM_ERROR;

  while (state >= 0) {
    pr = fsm->states[state].rules;
    n = fsm->states[state].nrules;
    state = -1;
    for (i = 0; i < n; i++, pr++)
      if ((pr->pred == fsm_char && pr->in == in) || pr->pred(in)) {
        state = pr->next;
        r = pr->fn ? pr->fn(in, context) : 0;
        if (r > 0)
          break;
        else if (r < 0)
          return FSM_ERROR;
        return pr->next;
      }
  }
  return FSM_ERROR;
}

struct fsm *
make_fsm(struct fsm_rule *rules)
{
  struct fsm *fsm;
  struct fsm_state *ps;
  int i, j, prev = -1, nstates = 0;

  for (i = 0; rules[i].pred; i++) {
    if (nstates < rules[i].state)
      nstates = rules[i].state;
    if (rules[i].state < prev) /* unsorted */
      return 0;
    prev = rules[i].state;
  }
  nstates++;

  fsm = (struct fsm *)malloc(sizeof(struct fsm) 
                             + sizeof(struct fsm_state) * nstates);
  if (!fsm)
    return 0;

  fsm->nstates = nstates;
  ps = fsm->states = (struct fsm_state *)(fsm + 1);

  for (i = 0; i < nstates; i++, ps++) {
    ps->nrules = 0;
    ps->rules = 0;
    for (j = 0; rules[j].pred; j++)
      if (rules[j].state == i) {
        ps->nrules++;
        if (!ps->rules)
          ps->rules = &rules[j];
      }
  }
  return fsm;
}

int
fsm_true(int in)
{
  return 1;
}

int
fsm_char(int in)
{
  return 0;
}

int
fsm_reject(int in, void *context)
{
  return 1;
}
