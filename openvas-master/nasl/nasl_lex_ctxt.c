/* Based on work Copyright (C) 2002 - 2004 Tenable Network Security
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "nasl_lex_ctxt.h"

#include "nasl_func.h"
#include "nasl_global_ctxt.h"
#include "nasl_tree.h"
#include "nasl_var.h"

#include <glib.h> /* for g_free() */

void
init_nasl_library (lex_ctxt *);

lex_ctxt *
init_empty_lex_ctxt ()
{
  lex_ctxt *c = g_malloc0 (sizeof (lex_ctxt));

  c->ctx_vars.hash_elt = g_malloc0 (sizeof (named_nasl_var *) * VAR_NAME_HASH);
  c->ctx_vars.num_elt = NULL;
  c->ctx_vars.max_idx = 0;
  c->functions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                        (GDestroyNotify) free_func);
  c->oid = NULL;
  c->ret_val = NULL;
  c->fct_ctxt = 0;

  /** @todo Initialization of the library seems intuitively be necessary only
   * once (involves "linking" the nasl functions to c code).  Consider a
   * "prototype" context that has to be created only once and of which copies
   * are made when needed. */
  init_nasl_library (c);

  return c;
}

void
free_lex_ctxt (lex_ctxt *c)
{
  deref_cell (c->ret_val);
  free_array (&c->ctx_vars);
  g_hash_table_destroy (c->functions);
  g_free (c);
}

void
dump_ctxt (lex_ctxt *c)
{
  int i;
  named_nasl_var *v;

  printf ("--------<CTXT>--------\n");
  if (c->fct_ctxt)
    printf ("Is a function context\n");
  if (c->up_ctxt == NULL)
    printf ("Is the top level context\n");
  if (c->ret_val)
    {
      printf ("Return value\n");
      nasl_dump_tree (c->ret_val);
    }

  printf ("Variables:\n");
  for (i = 0; i < VAR_NAME_HASH; i++)
    for (v = c->ctx_vars.hash_elt[i]; v != NULL; v = v->next_var)
      printf ("%s\t", v->var_name);
  putchar ('\n');

  printf ("----------------------\n");
}
