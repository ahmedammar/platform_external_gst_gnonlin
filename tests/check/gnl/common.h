
#include <gst/check/gstcheck.h>

#define fail_unless_equals_int64(a, b)					\
G_STMT_START {								\
  gint64 first = a;							\
  gint64 second = b;							\
  fail_unless(first == second,						\
    "'" #a "' (%" G_GINT64_FORMAT ") is not equal to '" #b"' (%"	\
    G_GINT64_FORMAT ")", first, second);				\
} G_STMT_END;

#define check_start_stop_duration(object, startval, stopval, durval)	\
  G_STMT_START { guint64 start, stop;					\
    gint64 duration;							\
    GST_DEBUG_OBJECT (object, "Checking for valid start/stop/duration values");					\
    g_object_get (object, "start", &start, "stop", &stop,		\
		  "duration", &duration, NULL);				\
    fail_unless_equals_uint64(start, startval);				\
    fail_unless_equals_uint64(stop, stopval);				\
    fail_unless_equals_int64(duration, durval);				\
    GST_DEBUG_OBJECT (object, "start/stop/duration values valid");	\
  } G_STMT_END;

#define check_state_simple(object, expected_state)			\
  G_STMT_START {							\
    GstStateChangeReturn ret;						\
    GstState state, pending;						\
    ret = gst_element_get_state(GST_ELEMENT_CAST(object), &state, &pending, 5 * GST_SECOND); \
    fail_if (ret == GST_STATE_CHANGE_FAILURE);				\
    fail_unless (state == expected_state, "Element state (%s) is not the expected one (%s)", \
		 gst_element_state_get_name(state), gst_element_state_get_name(expected_state)); \
  } G_STMT_END;

typedef struct _Segment {
  gdouble	rate;
  GstFormat	format;
  gint64	start, stop, position;
}	Segment;

typedef struct _CollectStructure {
  GstElement	*comp;
  GstElement	*sink;
  guint64	last_time;
  gboolean	gotsegment;
  GList		*expected_segments;
}	CollectStructure;

static GstElement *
gst_element_factory_make_or_warn (const gchar * factoryname, const gchar * name)
{
  GstElement * element;

  element = gst_element_factory_make (factoryname, name);
  fail_unless (element != NULL, "Failed to make element %s", factoryname);
  return element;
}

static void
composition_pad_added_cb (GstElement *composition, GstPad *pad, CollectStructure * collect)
{
  fail_if (!(gst_element_link_pads_full (composition, GST_OBJECT_NAME (pad), collect->sink, "sink",
					 GST_PAD_LINK_CHECK_NOTHING)));
}

/* return TRUE to discard the Segment */
static gboolean
compare_segments (Segment * segment, GstEvent * event)
{
  gboolean update;
  gdouble rate;
  GstFormat format;
  gint64 start, stop, position;

  gst_event_parse_new_segment (event, &update, &rate, &format, &start, &stop, &position);

  GST_DEBUG ("Got NewSegment update:%d, rate:%f, format:%d, start:%"GST_TIME_FORMAT
	     ", stop:%"GST_TIME_FORMAT", position:%"GST_TIME_FORMAT,
	     update, rate, format, GST_TIME_ARGS (start),
	     GST_TIME_ARGS (stop),
	     GST_TIME_ARGS (position));

  GST_DEBUG ("Expecting rate:%f, format:%d, start:%"GST_TIME_FORMAT
	     ", stop:%"GST_TIME_FORMAT", position:%"GST_TIME_FORMAT,
	     segment->rate, segment->format,
	     GST_TIME_ARGS (segment->start),
	     GST_TIME_ARGS (segment->stop),
	     GST_TIME_ARGS (segment->position));

  if (update) {
    GST_DEBUG ("was update, ignoring");
    return FALSE;
  }
  fail_if (rate != segment->rate);
  fail_if (format != segment->format);
  fail_unless_equals_int64 (start, segment->start);
  fail_unless_equals_int64 (stop, segment->stop);
  fail_unless_equals_int64 (position, segment->position);

  GST_DEBUG ("Segment was valid, discarding expected Segment");

  return TRUE;
}

static gboolean
sinkpad_event_probe (GstPad * sinkpad, GstEvent * event, CollectStructure * collect)
{
  Segment * segment;
  
  if (GST_EVENT_TYPE (event) == GST_EVENT_NEWSEGMENT) {
    fail_if (collect->expected_segments == NULL, "Received unexpected segment");
    segment = (Segment *) collect->expected_segments->data;

    if (compare_segments (segment, event)) {
      collect->expected_segments = g_list_remove (collect->expected_segments, segment);
      g_free (segment);
    }
    collect->gotsegment = TRUE;
  }

  return TRUE;
}

static gboolean
sinkpad_buffer_probe (GstPad * sinkpad, GstBuffer * buffer, CollectStructure * collect)
{
  fail_if(!collect->gotsegment);
  return TRUE;
}

static GstElement *
new_gnl_src (const gchar * name, guint64 start, gint64 duration, gint priority)
{
  GstElement * gnlsource = NULL;

  gnlsource = gst_element_factory_make_or_warn ("gnlsource", name);
  fail_if (gnlsource == NULL);

  g_object_set (G_OBJECT (gnlsource),
		"start", start,
		"duration", duration,
		"media-start", start,
		"media-duration", duration,
		"priority", priority,
		NULL);

  return gnlsource;
}

static GstElement *
videotest_gnl_src (const gchar * name, guint64 start, gint64 duration,
		   gint pattern, guint priority)
{
  GstElement * gnlsource = NULL;
  GstElement * videotestsrc = NULL;
  GstCaps * caps = gst_caps_from_string ("video/x-raw-yuv,format=(fourcc)I420,framerate=(fraction)3/2");

  fail_if (caps == NULL);

  videotestsrc = gst_element_factory_make_or_warn ("videotestsrc", NULL);
  g_object_set (G_OBJECT (videotestsrc), "pattern", pattern, NULL);

  gnlsource = new_gnl_src (name, start, duration, priority);
  g_object_set (G_OBJECT (gnlsource), "caps", caps, NULL);
  gst_caps_unref(caps);

  gst_bin_add (GST_BIN (gnlsource), videotestsrc);
  
  return gnlsource;
}

static GstElement *
videotest_gnl_src_full (const gchar * name, guint64 start, gint64 duration,
			guint64 mediastart, gint64 mediaduration,
			gint pattern, guint priority)
{
  GstElement * gnls;

  gnls = videotest_gnl_src (name, start, duration, pattern, priority);
  if (gnls) {
    g_object_set (G_OBJECT (gnls),
		  "media-start", mediastart,
		  "media-duration", mediaduration,
		  NULL);
  }

  return gnls;
}

static GstElement *
videotest_in_bin_gnl_src (const gchar * name, guint64 start, gint64 duration, gint pattern, guint priority)
{
  GstElement * gnlsource = NULL;
  GstElement * videotestsrc = NULL;
  GstElement * bin = NULL;
  GstElement * alpha = NULL;
  GstPad *srcpad = NULL;

  alpha = gst_element_factory_make ("alpha", NULL);
  if (alpha == NULL)
    return NULL;

  videotestsrc = gst_element_factory_make_or_warn ("videotestsrc", NULL);
  g_object_set (G_OBJECT (videotestsrc), "pattern", pattern, NULL);
  bin = gst_bin_new (NULL);

  gnlsource = new_gnl_src (name, start, duration, priority);

  gst_bin_add (GST_BIN (bin), videotestsrc);
  gst_bin_add (GST_BIN (bin), alpha);

  gst_element_link_pads_full (videotestsrc, "src", alpha, "sink",
			      GST_PAD_LINK_CHECK_NOTHING);

  gst_bin_add (GST_BIN (gnlsource), bin);
  
  srcpad = gst_element_get_pad (alpha, "src");

  gst_element_add_pad (bin, gst_ghost_pad_new ("src", srcpad));

  gst_object_unref (srcpad);

  return gnlsource;
}

static GstElement *
audiotest_bin_src (const gchar * name, guint64 start,
		   gint64 duration, guint priority, gboolean intaudio)
{
  GstElement * source = NULL;
  GstElement * identity = NULL;
  GstElement * audiotestsrc = NULL;
  GstElement * audioconvert = NULL;
  GstElement * bin = NULL;
  GstCaps * caps;
  GstPad *srcpad = NULL;

  audiotestsrc = gst_element_factory_make_or_warn ("audiotestsrc", NULL);
  identity = gst_element_factory_make_or_warn ("identity", NULL);
  bin = gst_bin_new (NULL);
  source = new_gnl_src (name, start, duration, priority);
  audioconvert = gst_element_factory_make_or_warn ("audioconvert", NULL);
  
  if (intaudio)
    caps = gst_caps_from_string ("audio/x-raw-int");
  else
    caps = gst_caps_from_string ("audio/x-raw-float");

  gst_bin_add_many (GST_BIN (bin), audiotestsrc, audioconvert, identity, NULL);
  gst_element_link_pads_full (audiotestsrc, "src", audioconvert, "sink",
			      GST_PAD_LINK_CHECK_NOTHING);
  fail_if ((gst_element_link_filtered (audioconvert, identity, caps)) != TRUE);

  gst_caps_unref (caps);

  gst_bin_add (GST_BIN (source), bin);

  srcpad = gst_element_get_pad (identity, "src");

  gst_element_add_pad (bin, gst_ghost_pad_new ("src", srcpad));

  gst_object_unref (srcpad);

  return source;
}

static GstElement *
new_operation (const gchar * name, const gchar * factory, guint64 start, gint64 duration, guint priority)
{
  GstElement * gnloperation = NULL;
  GstElement * operation = NULL;

  operation = gst_element_factory_make_or_warn (factory, NULL);
  gnloperation = gst_element_factory_make_or_warn ("gnloperation", name);

  g_object_set (G_OBJECT (gnloperation),
		"start", start,
		"duration", duration,
		"priority", priority,
		NULL);

  gst_bin_add (GST_BIN (gnloperation), operation);

  return gnloperation;
}


static Segment *
segment_new (gdouble rate, GstFormat format, gint64 start, gint64 stop, gint64 position)
{
  Segment * segment;

  segment = g_new0 (Segment, 1);

  segment->rate = rate;
  segment->format = format;
  segment->start = start;
  segment->stop = stop;
  segment->position = position;

  return segment;
}

static GList *
copy_segment_list (GList *list)
{
  GList *res = NULL;

  while (list) {
    Segment *pdata = (Segment*) list->data;

    res = g_list_append(res, segment_new(pdata->rate, pdata->format, pdata->start, pdata->stop, pdata->position));

    list = list->next;
  }

  return res;
}
