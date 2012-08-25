/* Copyright 2010-2011 Ramil Farkhshatov

This file is part of libxmpps.

libxmpps is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

libxmpps is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with libxmpps.  If not, see <http://www.gnu.org/licenses/>. */
#include <stdlib.h>
#include "fsm.h"

int
fsm_run(struct fsm *fsm, int in, int state, void *context)
{
  int i, n, r;
  struct fsm_rule *pr;

  while (state >= 0 && state < fsm->nstates) {
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
