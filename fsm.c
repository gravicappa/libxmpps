#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include "fsm.h"

int
fsm_run_rules(struct fsm_rule *rules, int in, int state, void *context)
{
  int i;

  while (1)
    for (i = 0; rules[i].pred || rules[i].in >= 0; i++)
      if (rules[i].state == state
          && ((rules[i].pred == fsm_char && rules[i].in == in)
              || rules[i].pred(in))) {
        if (rules[i].fn && rules[i].fn(in, context))
          break;
        return rules[i].next;
      }
  return FSM_ERROR;
}

void
fsm_print_rule(FILE *out, struct fsm_rule *r)
{
  fprintf(out, "rule %d", r->state);
  if (r->pred == fsm_char)
    fprintf(out, " c: '%c'", r->in);
  else if (r->pred == fsm_true)
    fprintf(out, " true");
  else if (r->pred == isalpha)
    fprintf(out, " isalpha");
  else if (r->pred == isspace)
    fprintf(out, " isspace");
  else if (r->pred == isalnum)
    fprintf(out, " isalnum");
  fprintf(out, " to: %d\n", r->next);
}

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
    for (i = 0; i < n; i++, pr++) {
      fprintf(stderr, ";; checking in: '%c' ", in);
      fsm_print_rule(stderr, pr);
      if ((pr->pred == fsm_char && pr->in == in) || pr->pred(in)) {
        fprintf(stderr, ";; matched in: '%c' ", in);
        fsm_print_rule(stderr, pr);
        state = pr->next;
        r = pr->fn ? pr->fn(in, context) : 0;
        if (r > 0) {
          fprintf(stderr, ";; breaking\n");
          break;
        } else if (r < 0)
          return FSM_ERROR;
        return pr->next;
      }
    }
  }
  return FSM_ERROR;
}

struct fsm *
make_fsm(struct fsm_rule *rules)
{
  struct fsm *fsm;
  int i, j, nrules, nstates, prev, m;
  struct fsm_state *ps;
  struct fsm_rule *pr;

  prev = -1;
  nrules = nstates = 0;
  for (i = 0; rules[i].pred; i++) {
    if (nstates < rules[i].state)
      nstates = rules[i].state;
    if (rules[i].state < prev) /* unsorted */
      return 0;
    prev = rules[i].state;
  }
  nstates++;
  nrules = i;

  fsm = (struct fsm *)malloc(sizeof(struct fsm) 
                             + sizeof(struct fsm_state) * nstates);
  if (!fsm)
    return 0;

  fsm->nstates = nstates;
  fsm->states = (struct fsm_state *)(fsm + 1);

  ps = fsm->states;
  for (i = 0; i < nstates; i++, ps++) {
    m = 0;
    pr = 0;
    for (j = 0; rules[j].pred; j++)
      if (rules[j].state == i) {
        m++;
        if (!pr)
          pr = &rules[j];
      }
    ps->nrules = m;
    ps->rules = pr;
  }
  return fsm;
}

void
print_fsm(struct fsm *fsm)
{
  struct fsm_state *s;
  int i;

  fprintf(stderr, "fsm states: %d\n", fsm->nstates);
  for (i = 0, s = fsm->states; i < fsm->nstates; i++, s++) {
    fprintf(stderr, "state: %d rules: %d\n", i, s->nrules);
  }
  fprintf(stderr, "end fsm\n");
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
