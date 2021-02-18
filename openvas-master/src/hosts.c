/* Portions Copyright (C) 2009-2020 Greenbone Networks GmbH
 * Portions Copyright (C) 2006 Software in the Public Interest, Inc.
 * Based on work Copyright (C) 1998 - 2006 Tenable Network Security, Inc.
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

/**
 * @file hosts.c
 * @brief Basically creates a new process for each tested host.
 */

#include "hosts.h" /* for hosts_new() */

#include "../misc/network.h" /* for internal_recv */
#include "utils.h"           /* for data_left() */

#include <errno.h>    /* for errno() */
#include <glib.h>     /* for g_free() */
#include <stdio.h>    /* for snprintf() */
#include <string.h>   /* for strlen() */
#include <sys/wait.h> /* for waitpid() */
#include <unistd.h>   /* for close() */

#undef G_LOG_DOMAIN
/**
 * @brief GLib log domain.
 */
#define G_LOG_DOMAIN "sd   main"

/**
 * @brief Host information, implemented as doubly linked list.
 */
struct host
{
  char *name;
  char *ip;
  pid_t pid;
  kb_t host_kb;
  kb_t results_kb;
  struct host *next;
  struct host *prev;
};
/** @TODO struct hosts could be stripped down and put in a g_list, or,
 *        as a g_hash_table (name -> [soc,pid]), see hosts_get.*/

static struct host *hosts = NULL;
static int g_max_hosts = 15;

/*-------------------------------------------------------------------*/
extern int global_scan_stop;

/**
 * @brief Add star_scan and end_scan results to the main kb.
 *
 * @param[in] kb    Main KB where results are stored.
 * @param[in] ip    List of vhosts to add new vhosts to.
 * @param[in] type  If it is start or end message.
 *
 */
void
host_set_time (kb_t kb, char *ip, char *type)
{
  char *timestr;
  char log_msg[1024];
  time_t t;
  int len;

  t = time (NULL);
  timestr = g_strdup (ctime (&t));
  len = strlen (timestr);
  if (timestr[len - 1] == '\n')
    timestr[len - 1] = '\0';

  snprintf (log_msg, sizeof (log_msg), "%s|||%s||||||||| |||%s", type, ip,
            timestr);
  g_free (timestr);

  kb_item_push_str (kb, "internal/results", log_msg);
}

static void
host_rm (struct host *h)
{
  if (h->pid != 0)
    waitpid (h->pid, NULL, WNOHANG);

  if (h->next != NULL)
    h->next->prev = h->prev;

  if (h->prev != NULL)
    h->prev->next = h->next;

  if (h->host_kb)
    {
      kb_delete (h->host_kb);
      h->host_kb = NULL;
      kb_lnk_reset (h->results_kb);
    }

  g_free (h->name);
  g_free (h);
}

/*-----------------------------------------------------------------*/

/**
 * @brief Returns the number of entries in the global hosts list.
 */
static int
hosts_num (void)
{
  struct host *h = hosts;
  int num;

  for (num = 0; h != NULL; num++, h = h->next)
    ;

  return num;
}

/**
 * @brief Retrieves a host specified by its name from the global host list.
 */
static struct host *
hosts_get (char *name)
{
  struct host *h = hosts;
  while (h != NULL)
    {
      if (strcmp (h->name, name) == 0)
        return h;
      h = h->next;
    }
  return NULL;
}

int
hosts_init (int max_hosts)
{
  g_max_hosts = max_hosts;
  return 0;
}

int
hosts_new (char *name, kb_t kb, kb_t main_kb)
{
  struct host *h;

  while (hosts_num () >= g_max_hosts)
    {
      if (hosts_read () < 0)
        return -1;
    }
  if (global_scan_stop)
    return 0;

  h = g_malloc0 (sizeof (struct host));
  h->name = g_strdup (name);
  h->pid = 0;
  h->host_kb = kb;
  h->results_kb = main_kb;
  if (hosts != NULL)
    hosts->prev = h;
  h->next = hosts;
  h->prev = NULL;
  hosts = h;
  return 0;
}

int
hosts_set_pid (char *name, pid_t pid)
{
  struct host *h = hosts_get (name);
  if (h == NULL)
    {
      g_debug ("host_set_pid() failed!\n");
      return -1;
    }

  h->pid = pid;
  return 0;
}

/*-----------------------------------------------------------------*/
static int
hosts_stop_host (struct host *h)
{
  if (h == NULL)
    return -1;

  g_message ("Stopping host %s scan (pid: %d)", h->name, h->pid);
  kill (h->pid, SIGUSR1);
  return 0;
}

void
hosts_stop_all (void)
{
  struct host *host = hosts;

  global_scan_stop = 1;
  while (host)
    {
      hosts_stop_host (host);
      host = host->next;
    }
}

/*-----------------------------------------------------------------*/

static void
hosts_read_data (void)
{
  struct host *h = hosts;
  int ret = 1;

  while (ret > 0)
    {
      ret = waitpid (-1, NULL, WNOHANG);
      if (ret < 0)
        g_debug ("waitpid() failed. %s)", strerror (errno));
    }

  if (h == NULL)
    return;

  while (h)
    {
      if (h->pid != 0 && kill (h->pid, 0) < 0) /* Process is dead */
        {
          if (!h->prev)
            hosts = hosts->next;
          host_rm (h);
          h = hosts;
          if (!h)
            break;
        }
      h = h->next;
    }
}

/**
 * @brief Returns -1 if client asked to stop all tests or connection was lost or
 * error. 0 otherwise.
 */
int
hosts_read (void)
{
  if (hosts == NULL)
    return -1;

  hosts_read_data ();
  usleep (500000);

  return 0;
}
