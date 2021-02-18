/* Copyright (C) 2019 Greenbone Networks GmbH
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#include "attack.c"

#include <cgreen/cgreen.h>
#include <cgreen/mocks.h>

Describe (attack);
BeforeEach (attack)
{
}
AfterEach (attack)
{
}

/* comm_send_status */

gchar *given_name = NULL;
gchar *given_value = NULL;

int
__wrap_redis_push_str (kb_t kb, const char *name, const char *value)
{
  (void) kb; /* Used. */
  given_name = g_strdup (name);
  given_value = g_strdup (value);
  mock ();
  return 0;
}

Ensure (attack, comm_send_status_returns_neg1_for_null_args)
{
  struct kb kb_struct;
  kb_t kb;

  /* Create a dummy kb. */
  kb = &kb_struct;

  never_expect (__wrap_redis_push_str);
  assert_that (comm_send_status (NULL, "example", 0, 100), is_equal_to (-1));
  assert_that (comm_send_status (kb, NULL, 0, 100), is_equal_to (-1));
}

Ensure (attack, comm_send_status_error_if_hostname_too_big)
{
  struct kb kb_struct;
  kb_t kb;
  gchar *long_host;
  int index;

  /* Create a dummy kb. */
  kb = &kb_struct;

  long_host = g_malloc (2049);
  for (index = 0; index < 2049; index++)
    long_host[index] = 'a';
  long_host[2048] = '\0';

  never_expect (__wrap_redis_push_str);
  assert_that (comm_send_status (kb, long_host, 0, 100), is_equal_to (-1));

  g_free (long_host);
}

Ensure (attack, comm_send_status_sends_correct_text)
{
  struct kb kb_struct;
  struct kb_operations kb_ops_struct;
  kb_t kb;

  /* Create a dummy kb. */
  kb = &kb_struct;

  /* We can't wrap kb_item_push_str because it is inline, so we have to do
   * a little hacking. */
  kb_ops_struct.kb_push_str = __wrap_redis_push_str;
  kb->kb_ops = &kb_ops_struct;

  expect (__wrap_redis_push_str);
  assert_that (comm_send_status (kb, "127.0.0.1", 11, 67), is_equal_to (0));
  assert_that (strcmp (given_name, "internal/status"), is_equal_to (0));
  assert_that (strcmp (given_value, "127.0.0.1/11/67"), is_equal_to (0));

  g_free (given_name);
  g_free (given_value);
}

int
main (int argc, char **argv)
{
  TestSuite *suite;

  suite = create_test_suite ();

  add_test_with_context (suite, attack,
                         comm_send_status_returns_neg1_for_null_args);
  add_test_with_context (suite, attack,
                         comm_send_status_error_if_hostname_too_big);
  add_test_with_context (suite, attack, comm_send_status_sends_correct_text);

  if (argc > 1)
    return run_single_test (suite, argv[1], create_text_reporter ());

  return run_test_suite (suite, create_text_reporter ());
}
