
#include <string.h>

#include <unistd.h>

#include <nice/nice.h>

static gboolean cb_called = FALSE;

static void
handle_recv (
  NiceAgent *agent,
  guint stream_id,
  guint component_id,
  guint len,
  gchar *buf,
  gpointer data)
{
  g_assert (agent != NULL);
  g_assert (cb_called == FALSE);
  g_assert (stream_id == 1);
  g_assert (component_id == 1);
  g_assert (len == 7);
  g_assert (0 == strncmp (buf, "\x80lalala", 7));
  g_assert (GPOINTER_TO_UINT (data) == 42);
  cb_called = TRUE;
}

int
main (void)
{
  NiceAgent *agent;
  NiceAddress addr = {0,};
  NiceUDPSocketFactory factory;
  NiceUDPSocket *sock;
  gint pipe_fds[2];
  GSList *fds = NULL;
  GSList *readable;

  g_type_init ();

  /* set up agent */

  if (!nice_address_set_ipv4_from_string (&addr, "127.0.0.1"))
    g_assert_not_reached ();

  nice_udp_fake_socket_factory_init (&factory);
  agent = nice_agent_new (&factory);
  nice_agent_add_local_address (agent, &addr);
  nice_agent_add_stream (agent, 1);

      {
        GSList *candidates;
        NiceCandidate *candidate;

        candidates = nice_agent_get_local_candidates (agent, 1, 1);
        candidate = candidates->data;
        sock = &candidate->sock;
        g_slist_free (candidates);
      }

  /* set up pipe and fd list */

  if (pipe (pipe_fds) != 0)
    g_assert_not_reached ();

  write (pipe_fds[1], "hello", 5);

  fds = g_slist_append (fds, GUINT_TO_POINTER (pipe_fds[0]));

  /* poll */

  readable = nice_agent_poll_read (agent, fds, NULL, NULL);
  g_assert (g_slist_length (readable) == 1);
  g_assert (GPOINTER_TO_UINT (readable->data) == (guint) pipe_fds[0]);
  g_slist_free (readable);
  g_assert (cb_called == FALSE);

   {
     gchar buf[1024];

     g_assert (5 == read (pipe_fds[0], buf, 1024));
     g_assert (0 == strncmp (buf, "hello", 5));
   }

  /* send fake data */

  nice_udp_fake_socket_push_recv (sock, &addr, 7, "\x80lalala");

  /* poll again */

  readable = nice_agent_poll_read (agent, fds, handle_recv,
      GUINT_TO_POINTER (42));
  g_assert (cb_called == TRUE);
  g_assert (readable == NULL);

  /* clean up */

  g_slist_free (fds);
  g_object_unref (agent);

  return 0;
}

