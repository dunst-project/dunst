/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "notification.h"

#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>

#include "dbus.h"
#include "dunst.h"
#include "icon.h"
#include "log.h"
#include "markup.h"
#include "menu.h"
#include "queues.h"
#include "rules.h"
#include "settings.h"
#include "utils.h"
#include "draw.h"
#include "icon-lookup.h"

static void notification_extract_urls(struct notification *n);
static void notification_format_message(struct notification *n);

/* see notification.h */
const char *enum_to_string_fullscreen(enum behavior_fullscreen in)
{
        switch (in) {
        case FS_SHOW: return "show";
        case FS_DELAY: return "delay";
        case FS_PUSHBACK: return "pushback";
        case FS_NULL: return "(null)";
        default:
                LOG_E("Invalid %s enum value in %s:%d", "fullscreen", __FILE__, __LINE__);
                break;
        }
}

struct _notification_private {
        gint refcount;
};

extern char **environ;


//TODO: Move to utils.c but for now I wanna edit only the files I have to...
#define ESCAPE_NEWLINES(VAR)    string_replace_all("\n", "\\n", string_replace_all("\r", "", g_strdup(VAR)))
#define UNESCAPE_NEWLINES(VAR)  string_replace_all("\\n", "\n", g_strdup(VAR))
#define SCRIPT_BUFFER_LEN       32*1024 // [b] how big should the MMAP region be

//TODO: Move to utils.c but for now I wanna edit only the files I have to...
#define STR_HEXDUMP(VAR)        hexdump(VAR, strlen(VAR))
void hexdump(char * buffer, int len) {
    int x;

    char linebuff_x[256] = { 0 };
    char linebuff_c[32] = { 0 };
    int pos_x = 0;
    int pos_c = 0;

    for( x=0 ; x<=len ; x++ ) {
        pos_x += snprintf( linebuff_x+pos_x, 256-pos_x, "%02X ", (unsigned char) buffer[x] );
        pos_c += snprintf( linebuff_c+pos_c, 32-pos_x, "%c", buffer[x] <= ' ' ? '.' : (unsigned char) buffer[x] );

        if( !((x+1) % 8) || x+1>=len ) {
            // pad hex to full length...
            for( ; (x+1) % 8 ; x++ ) strcat(linebuff_x, "   ");

            printf("%s  | %s\n", linebuff_x, linebuff_c);
            pos_x = 0;
            pos_c = 0;
        }
    }
}



/* see notification.h */
void notification_print(const struct notification *n)
{
        // Nice & readable :-) It was not valid JSON in the first place, so we
        // can shave a couple of newlines here and there. And we can skip unset
        // or empty variables. Fewer lines = more readable logs that don't scroll
        // away at the speed of light.

        char timeout_buffer[32] = { 0 };
        if( n->timeout>=1000000 ) {
            snprintf(timeout_buffer, 32, "%lds", n->timeout/1000000);
        } else {
            snprintf(timeout_buffer, 32, "%ldms", n->timeout/1000);
        }

        LOG_I("Notification  [ID=%d  TIMEOUT=%s  URGENCY=%s  TRANSIENT=%c  FULLSCREEN=%s]",
              n->id,
              timeout_buffer,
              notification_urgency_to_string(n->urgency),
              n->transient ? 'Y' : 'N',
              enum_to_string_fullscreen(n->fullscreen));
        LOG_I("{");

        // Most important things first: source identification
        LOG_I("\tappname: '%s'", n->appname);
        if( !STR_EMPTY(n->desktop_entry) )
            LOG_I("\tdesktop_entry: '%s'", n->desktop_entry);

        // Followed by important stuff such as title/text
        LOG_I("\tsummary: '%s'", n->summary);
        if( !STR_EMPTY(n->body) )
            LOG_I("\tbody: '%s'", n->body);
        LOG_I("\tformat: %s", n->format);
        LOG_I("\tformatted: '%s'", n->msg);

        // Less important stuff, special params etc
        if( !STR_EMPTY(n->category) )
            LOG_I("\tcategory: %s", n->category);
        if( n->progress >= 0 )
            LOG_I("\tprogress: %d", n->progress);
        if( !STR_EMPTY(n->stack_tag) )
            LOG_I("\tstack_tag: %s",n->stack_tag);

        // Eye candy goes to the end (almost)
        LOG_I("\ticon: '%s'", n->iconname);
        LOG_I("\traw_icon set: %c", (n->icon_id && !STR_EQ(n->iconname, n->icon_id)) ? 'Y' : 'N');
        if( !STR_EMPTY(n->icon_id) )
            LOG_I("\ticon_id: '%s'", n->icon_id);

        LOG_I("\tfg: %s", n->colors.fg);
        LOG_I("\tbg: %s", n->colors.bg);
        LOG_I("\thighlight: %s", n->colors.highlight);
        LOG_I("\tframe: %s", n->colors.frame);

        // Lists at the end
        if (n->urls) {
                char *urls = string_replace_all("\n", "\t\t\n", g_strdup(n->urls));
                LOG_I("\turls:");
                LOG_I("\t{");
                LOG_I("\t\t%s", urls);
                LOG_I("\t}");
                g_free(urls);
        }

        if (g_hash_table_size(n->actions) > 0) {
                gpointer p_key, p_value;
                GHashTableIter iter;
                g_hash_table_iter_init(&iter, n->actions);
                LOG_I("\tactions: {");
                while (g_hash_table_iter_next(&iter, &p_key, &p_value))
                        LOG_I("\t\t\"%s\": \"%s\"", (char*)p_key, (char*)p_value);
                LOG_I("\t}");
        }

        if (n->script_count > 0) {
                LOG_I("\tscripts: {");
                for (int i = 0; i < n->script_count; i++) {
                        LOG_I("\t\t'%s'",n->scripts[i]);
                }
                LOG_I("\t}");
        }
        LOG_I("}");
}

// Macro used in script_handle_env_line(). We have many similar blocks and this
// way we don't have to copy-paste that much. Final solution might be something
// like a table of script variables paired to notification struct members but
// for now this is good enough...
// Condition used here handles NULL and empty string as the same
#define OVERRIDE_IF_DIFFERENT_STR(N, NAME, VALUE)                           \
    if ( !STR_EQ(N->NAME, VALUE) && (N->NAME && strlen(VALUE)) ) {          \
        LOG_I("Script overrides "#NAME": '%s' -> '%s'", N->NAME, VALUE);    \
        if( N->NAME ) g_free( N->NAME );                                    \
        N->NAME = g_strndup(VALUE, DUNST_NOTIF_MAX_CHARS);                  \
        ret = 1;                                                            \
    }


int script_handle_env_line( struct notification *n, const char * line ) {
        char envName[128] = { 0 };
        char *en = envName;
        char envValue_buff[1024] = { 0 };
        char *ev = envValue_buff;
        const char *lb = line;
        int ret = 0;
        char *c = NULL;

        LOG_D("Parsing script ENV line: '%s'", line);

        // Now we can indulge in some pointer porn ^_^
        // Skip any leading whitespaces & control chars:
        // |   VAR="VALUE VALUE"
        //  ^^^
        while( *lb <= ' ' ) lb++;

        // Copy variable name (stop on '=')
        // |VAR="VALUE VALUE"
        //  ^^^
        while( *lb && *lb != '=') *en++ = *lb++;

        // Check that line is atleast somewhat valid by checking that 1) it has more
        // data, 2) variable name is not empty and 3) that next character is '"' by
        // advancing buffer and skipping over =
        // |="VALUE VALUE"
        //  ^
        if( !*lb || !strlen(envName) || *++lb != '"' ) {
                LOG_W("Ignoring invalid script line: '%s'", line);
                return 0;
        }

        // Skip "
        // |"VALUE VALUE"
        //  ^
        lb++;

        // Copy everything to the end of the line
        // |VALUE VALUE"
        //  ^^^^^^^^^^^^
        while( *lb ) *ev++ = *lb++;

        // Trim last quotes
        *--ev = 0;

        LOG_D("Script setting %s='%s'", envName, envValue_buff);


        // Get rid of double-escapes. Unescaping has to be done individually as
        // some fields need escaped (format) and some raw (body, summary) data.
        char * envValue = string_replace_all("\\\\", "\\", g_strdup(envValue_buff));


        if( STR_EQ("DUNST_TIMEOUT", envName) ) {
                gint64 v = g_ascii_strtoll(envValue, NULL, 10);
                if (!v) {
                        LOG_W("Script tried to set DUNST_TIMEOUT to invalid value (%s), ignoring", envValue);
                } else {
                        if (n->timeout/1000 != v) {
                                LOG_I("Script overrides TIMEOUT: %ld -> %ld", n->timeout / 1000, v);
                                n->timeout = v * 1000;
                                ret = 1;
                        }
                }

        } else if( STR_EQ("DUNST_PROGRESS", envName) ) {
                int v = (int) g_ascii_strtoll(envValue, NULL, 10);
                if (n->progress != v) {
                        LOG_I("Script overrides PROGRESS: %d -> %d", n->progress, v);
                        n->progress = v;
                        ret = 1;
                }

        }  else if( STR_EQ("DUNST_TRANSIENT", envName) ) {
                bool v = (g_ascii_strtoll(envValue, NULL, 10) > 0);
                if (n->transient != v) {
                        LOG_I("Script overrides TRANSIENT: %d -> %d", n->transient, v);
                        n->transient = v;
                        ret = 1;
                }

        } else if( STR_EQ("DUNST_SUMMARY", envName) ) {
                c = UNESCAPE_NEWLINES(envValue);
                OVERRIDE_IF_DIFFERENT_STR(n, summary, c);

        } else if( STR_EQ("DUNST_BODY", envName) ) {
                c = UNESCAPE_NEWLINES(envValue);
                OVERRIDE_IF_DIFFERENT_STR(n, body, c);

        } else if( STR_EQ("DUNST_FORMAT", envName) ) {
        // Took me 3 goddamn hours to figure out that format is not allocated per
        // notification - it is a pointer to some global value. And if you free
        // it, you will screw up all the following notifications... FML!
        if ( !STR_EQ(n->format, envValue) && (n->format && strlen(envValue)) ) {
                LOG_I("Script overrides format: '%s' -> '%s'",n->format, envValue);
                n->format = g_strndup(envValue, DUNST_NOTIF_MAX_CHARS);
                ret = 1;
        }

        } else if( STR_EQ("DUNST_CATEGORY", envName) ) {
                OVERRIDE_IF_DIFFERENT_STR(n, category, envValue);

        } else if( STR_EQ("DUNST_STACK_TAG", envName) ) {
                OVERRIDE_IF_DIFFERENT_STR(n, stack_tag, envValue);

        } else if( STR_EQ("DUNST_ICON_PATH", envName) ) {
                OVERRIDE_IF_DIFFERENT_STR(n, icon_path, envValue);

        }  else if( STR_EQ("DUNST_URGENCY", envName) ) {
                enum urgency u = URG_NONE;

                // Dictionary would be nice, but hey...
                if( STR_EQ("LOW", envValue) ) u = URG_LOW;
                else if( STR_EQ("NORMAL", envValue) ) u = URG_NORM;
                else if( STR_EQ("CRITICAL", envValue) ) u = URG_CRIT;
                else {
                        LOG_W("Script passed invalid value in DUNST_URGENCY: '%s'", envValue);
                }

        if( u != URG_NONE && n->urgency != u) {
                LOG_I("Script overrides URGENCY: %s -> %s",
                        notification_urgency_to_string(n->urgency),
                        notification_urgency_to_string(u));
                        n->urgency = u;
                ret = 1;
        }

        } else if( STR_EQ("DUNST_ID", envName) ||
               STR_EQ("DUNST_URLS", envName) ||
               STR_EQ("DUNST_DESKTOP_ENTRY", envName) ||
               STR_EQ("DUNST_APP_NAME", envName) ||
            STR_EQ("DUNST_TIMESTAMP", envName) ) {
        // Quietly ignore read-only properties...
        } else {
                LOG_W("Script tried to set unknown property %s to '%s' (ignoring)", envName, envValue);
        }

        g_free(envValue);
        if( c ) g_free(c);
        return ret;
}


int script_handle_env_buffer( struct notification *n, const unsigned char * buffer ) {
        char line_buffer[1024] = {0 };
        int line_buffer_pos = 0;
        int changes = 0;

        if( STR_EMPTY(buffer) )
                return 0;

        // Chop buffer to individual lines
        while( *buffer ) {
                // Handle line
                if((*buffer == '\n' || *buffer == '\r') && line_buffer_pos ) {
                        // Terminate buffer, so we don't need to memset it to 0 every line
                        line_buffer[line_buffer_pos] = 0;

                        if( script_handle_env_line(n, line_buffer) )
                                changes++;

                        line_buffer_pos = 0;
                        continue;
                }

                if(line_buffer_pos >= sizeof(line_buffer)-1 ) {
                        LOG_W("Script line_buffer overflow - dropping line!");
                        line_buffer_pos = 0;
                        continue;
                }

                // Skip any control chars (keep whitespaces)
                if( *buffer >= ' ' )
                        line_buffer[line_buffer_pos++] = *buffer;

                buffer++;
        }

        return changes;
}


/* see notification.h */
//TODO: *caller is only temporary for ease of debugging, will be removed
//TODO: Add script timeout - perhaps global config?
//TODO: Deprecate the ARGS style in the future? It is quite unsafe, no escaping etc...
int __notification_run_script(struct notification *n, bool blocking, const char *caller)
{
        //TODO: delete this, just for me to quickly untangle where is this being
        //      called from etc...
        printf("run_script called by: %s()\n", caller);

        if (n->script_run && !settings.always_run_script)
                return 0;

        n->script_run = true;

        int status;
        const char *appname = n->appname ? n->appname : "";
        const char *summary = n->summary ? n->summary : "";
        const char *body = n->body ? n->body : "";
        const char *icon = n->iconname ? n->iconname : "";

        // Our memory-mapped dirty secret
        unsigned char * script_output;

        // Allocate shared memory map, so we can pass data from our forked children. Of
        // course, we could do this nicer - with pthreads for example, but as a simple
        // proof-of-concept this should be quite ok. We are allocating static amount of
        // memory, which of course is not ideal etc.
        // Btw don't need to memset to 0 because MAP_ANONYMOUS does this for us :)
        script_output = (unsigned char *) mmap(0, SCRIPT_BUFFER_LEN,
                                               PROT_READ|PROT_WRITE,
                                               MAP_SHARED|MAP_ANONYMOUS,
                                               -1, 0);

        if( script_output == MAP_FAILED ) {
                LOG_E("Failed to allocate %d bytes of shared memory for script output!", SCRIPT_BUFFER_LEN);
        }


        for(int i = 0; i < n->script_count; i++) {
                const char *script = n->scripts[i];

                if (STR_EMPTY(script))
                        continue;

                LOG_I("Firing %sblocking script: %s", (blocking ? "" : "non-"), script);

                gint64 script_start = time_monotonic_now();

                int pid1 = fork();
                if (pid1) {
                        waitpid(pid1, &status, 0);
                        gint64 script_duration = (time_monotonic_now()-script_start)/1000;
                        LOG_I("Script '%s' returned %d in %ldms - received %zu bytes",
                              script,
                              WEXITSTATUS(status),
                              script_duration,
                              strlen((char*) script_output));

                        if( script_duration > 3000 ) {
                                LOG_W("Script '%s' took quite long (%ldms) to finish!",
                                        script,
                                        script_duration);
                                LOG_W("Please check the script as slow scripts will delay notifications");
                        }

                        // If script changed notification we need to handle reformatting etc
                        if( script_handle_env_buffer( n, script_output ) > 0 ) {
                                LOG_I("Script made some changes, will re-format the message");
                                notification_format_message(n);
                        }


                        // If script returned 1, it means that we should drop the notification
                        // outright. So we stop processing scripts in that case and just quit.
                        if( WEXITSTATUS(status) != 0 ) {
                                munmap(script_output, SCRIPT_BUFFER_LEN);
                                return 1;
                        }

                        continue;
                }

                // second fork to prevent zombie processes
                int pid2 = fork();
                if (pid2) {
                        // Non-blocking scripts can do whatever they want, and
                        // we don't care about the result. Fire & forget
                        if( !blocking ) exit(0);

                        // Blocking scripts, however, we do care about. We need
                        // to wait for them to finish and get results
                        waitpid(pid2, &status, 0);

                        // Propagate return value back to first fork
                        exit(WEXITSTATUS(status));
                }

                // Set environment variables
                gchar *n_id_str = g_strdup_printf("%i", n->id);
                gchar *n_progress_str = g_strdup_printf("%i", n->progress);
                gchar *n_timeout_str = g_strdup_printf("%li", n->timeout/1000);
                gchar *n_timestamp_str = g_strdup_printf("%li", n->timestamp / 1000);
                const char *urgency = notification_urgency_to_string(n->urgency);

                safe_setenv("DUNST_APP_NAME",  ESCAPE_NEWLINES(appname));
                safe_setenv("DUNST_SUMMARY",   ESCAPE_NEWLINES(summary));
                safe_setenv("DUNST_FORMAT",    n->format);
                safe_setenv("DUNST_BODY",      ESCAPE_NEWLINES(body));
                safe_setenv("DUNST_ICON_PATH", n->icon_path);
                safe_setenv("DUNST_URGENCY",   urgency);
                safe_setenv("DUNST_ID",        n_id_str);
                safe_setenv("DUNST_PROGRESS",  n_progress_str);
                safe_setenv("DUNST_CATEGORY",  n->category);
                safe_setenv("DUNST_STACK_TAG", n->stack_tag);
                safe_setenv("DUNST_URLS",      n->urls);
                safe_setenv("DUNST_TIMEOUT",   n_timeout_str);
                safe_setenv("DUNST_TIMESTAMP", n_timestamp_str);
                safe_setenv("DUNST_STACK_TAG", n->stack_tag);
                safe_setenv("DUNST_DESKTOP_ENTRY",  n->desktop_entry);
                safe_setenv("DUNST_TRANSIENT",      n->transient ? "1" : "0");


                // Now there is multiple ways to skin a cat. Either way the cat is not going to like it.
                // We can do full-blown IPC with all the bells and whistles, but it would be quite a lot
                // of work and error-prone. Or we can go for some BASH-magic & extract env variables as
                // strings and parse them. That should be easier if we constrain our parser with some
                // basic rules:
                //  1) Unknown lines are ignored
                //  2) Invalid values and parse errors = line ignored
                //  3) Only single-line variables (for now). You want multi = you escape it
                char script_command[1024] = { 0 };
                char lbuff[512] = { 0 };
                size_t lbuff_pos = 0;

                snprintf(script_command, sizeof(script_command),
                         "(. %s  '%s' '%s' '%s' '%s' '%s' && export) | grep -o 'DUNST_.*$'",
                         script,
                         ESCAPE_NEWLINES(appname),
                         ESCAPE_NEWLINES(summary),
                         ESCAPE_NEWLINES(body),
                         icon,
                         urgency);
                LOG_D("Command[%lu]: %s", strlen(script_command), script_command);

                FILE *proc = popen(script_command, "r");

                // Failure to run script = ignore it and carry on
                if( proc == NULL ) {
                        LOG_W("Unable to run script %s: %s", n->scripts[i], strerror(errno));
                        exit(0);
                }

                // Multiple scripts can be run so need to clear up after every one...
                memset(script_output, 0, SCRIPT_BUFFER_LEN);

                // Copy script output to shared memory. Simple string appends, not much love
                // or care given here. Just pump the data as fast as possible.
                while( fgets(lbuff, sizeof(lbuff), proc) != NULL ) {
                        lbuff_pos += strlen(lbuff);
                        strncat((char *) script_output, lbuff, SCRIPT_BUFFER_LEN-lbuff_pos);
                }

                // We only care whether script returned 0 or not.
                status = pclose(proc);

                // If script overflows, we just drop everything. It'd be foolish to try to make
                // sense from incomplete data as the result would most likely not be what the
                // user expects...
                if( lbuff_pos != strlen((char*) script_output) ) {
                        LOG_W("Data from script overflowed (%zu bytes), ignoring script completely!", lbuff_pos);
                        exit(0);
                }

                exit(status != 0);
        }

        munmap(script_output, SCRIPT_BUFFER_LEN);
        return 0;
}

/*
 * Helper function to convert an urgency to a string
 */
const char *notification_urgency_to_string(const enum urgency urgency)
{
        switch (urgency) {
        case URG_NONE:
                return "NONE";
        case URG_LOW:
                return "LOW";
        case URG_NORM:
                return "NORMAL";
        case URG_CRIT:
                return "CRITICAL";
        default:
                return "UNDEF";
        }
}

/* see notification.h */
int notification_cmp(const struct notification *a, const struct notification *b)
{
        if (settings.sort && a->urgency != b->urgency) {
                return b->urgency - a->urgency;
        } else {
                return a->id - b->id;
        }
}

/* see notification.h */
int notification_cmp_data(const void *va, const void *vb, void *data)
{
        struct notification *a = (struct notification *) va;
        struct notification *b = (struct notification *) vb;

        return notification_cmp(a, b);
}

bool notification_is_duplicate(const struct notification *a, const struct notification *b)
{
        return STR_EQ(a->appname, b->appname)
            && STR_EQ(a->summary, b->summary)
            && STR_EQ(a->body, b->body)
            && (a->icon_position != ICON_OFF ? STR_EQ(a->icon_id, b->icon_id) : 1)
            && a->urgency == b->urgency;
}

bool notification_is_locked(struct notification *n) {
        assert(n);

        return g_atomic_int_get(&n->locked) != 0;
}

struct notification* notification_lock(struct notification *n) {
        assert(n);

        g_atomic_int_set(&n->locked, 1);
        notification_ref(n);

        return n;
}

struct notification* notification_unlock(struct notification *n) {
        assert(n);

        g_atomic_int_set(&n->locked, 0);
        notification_unref(n);

        return n;
}

static void notification_private_free(NotificationPrivate *p)
{
        g_free(p);
}

/* see notification.h */
gint notification_refcount_get(struct notification *n)
{
        assert(n->priv->refcount > 0);
        return g_atomic_int_get(&n->priv->refcount);
}

/* see notification.h */
void notification_ref(struct notification *n)
{
        assert(n->priv->refcount > 0);
        g_atomic_int_inc(&n->priv->refcount);
}

/* see notification.h */
void notification_unref(struct notification *n)
{
        ASSERT_OR_RET(n,);

        assert(n->priv->refcount > 0);
        if (!g_atomic_int_dec_and_test(&n->priv->refcount))
                return;

        g_free(n->appname);
        g_free(n->summary);
        g_free(n->body);
        g_free(n->iconname);
        g_free(n->default_icon_name);
        g_free(n->icon_path);
        g_free(n->msg);
        g_free(n->dbus_client);
        g_free(n->category);
        g_free(n->text_to_render);
        g_free(n->urls);
        g_free(n->colors.fg);
        g_free(n->colors.bg);
        g_free(n->colors.highlight);
        g_free(n->colors.frame);
        g_free(n->stack_tag);
        g_free(n->desktop_entry);

        g_hash_table_unref(n->actions);
        g_free(n->default_action_name);

        if (n->icon)
                cairo_surface_destroy(n->icon);
        g_free(n->icon_id);

        notification_private_free(n->priv);

        if (n->script_count > 0){
                g_free(n->scripts);
        }

        g_free(n);
}

void notification_transfer_icon(struct notification *from, struct notification *to)
{
        if (from->iconname && to->iconname
                        && strcmp(from->iconname, to->iconname) == 0){
                // Icons are the same. Transfer icon surface
                to->icon = from->icon;

                // prevent the surface being freed by the old notification
                from->icon = NULL;
        }
}

void notification_icon_replace_path(struct notification *n, const char *new_icon)
{
        ASSERT_OR_RET(n,);
        ASSERT_OR_RET(new_icon,);
        if(n->iconname && n->icon && strcmp(n->iconname, new_icon) == 0) {
                return;
        }

        // make sure it works, even if n->iconname is passed as new_icon
        if (n->iconname != new_icon) {
                g_free(n->iconname);
                n->iconname = g_strdup(new_icon);
        }

        cairo_surface_destroy(n->icon);
        n->icon = NULL;
        g_clear_pointer(&n->icon_id, g_free);

        g_free(n->icon_path);
        n->icon_path = get_path_from_icon_name(new_icon, n->min_icon_size);
        if (n->icon_path) {
                GdkPixbuf *pixbuf = get_pixbuf_from_file(n->icon_path,
                                n->min_icon_size, n->max_icon_size,
                                draw_get_scale());
                if (pixbuf) {
                        n->icon = gdk_pixbuf_to_cairo_surface(pixbuf);
                        g_object_unref(pixbuf);
                } else {
                        LOG_W("No icon found in path: '%s'", n->icon_path);
                }
        }
}

void notification_icon_replace_data(struct notification *n, GVariant *new_icon)
{
        ASSERT_OR_RET(n,);
        ASSERT_OR_RET(new_icon,);

        cairo_surface_destroy(n->icon);
        n->icon = NULL;
        g_clear_pointer(&n->icon_id, g_free);

        GdkPixbuf *icon = icon_get_for_data(new_icon, &n->icon_id,
                        draw_get_scale(), n->min_icon_size, n->max_icon_size);
        n->icon = gdk_pixbuf_to_cairo_surface(icon);
        if (icon)
                g_object_unref(icon);
}

/* see notification.h */
void notification_replace_single_field(char **haystack,
                                       char **needle,
                                       const char *replacement,
                                       enum markup_mode markup_mode)
{

        assert(*needle[0] == '%');
        // needle has to point into haystack (but not on the last char)
        assert(*needle >= *haystack);
        assert(*needle - *haystack < strlen(*haystack) - 1);

        int pos = *needle - *haystack;

        char *input = markup_transform(g_strdup(replacement), markup_mode);
        *haystack = string_replace_at(*haystack, pos, 2, input);

        // point the needle to the next char
        // which was originally in haystack
        *needle = *haystack + pos + strlen(input);

        g_free(input);
}

static NotificationPrivate *notification_private_create(void)
{
        NotificationPrivate *priv = g_malloc0(sizeof(NotificationPrivate));
        g_atomic_int_set(&priv->refcount, 1);

        return priv;
}

/* see notification.h */
struct notification *notification_create(void)
{
        struct notification *n = g_malloc0(sizeof(struct notification));

        n->priv = notification_private_create();

        /* Unparameterized default values */
        n->first_render = true;
        n->markup = MARKUP_FULL;
        n->format = settings.format;

        n->timestamp = time_monotonic_now();

        n->urgency = URG_NORM;
        n->timeout = -1;
        n->dbus_timeout = -1;

        n->transient = false;
        n->progress = -1;
        n->word_wrap = true;
        n->ellipsize = PANGO_ELLIPSIZE_MIDDLE;
        n->alignment = PANGO_ALIGN_LEFT;
        n->progress_bar_alignment = PANGO_ALIGN_CENTER;
        n->hide_text = false;
        n->icon_position = ICON_LEFT;
        n->min_icon_size = 32;
        n->max_icon_size = 32;
        n->receiving_raw_icon = false;

        n->script_run = false;
        n->dbus_valid = false;

        n->fullscreen = FS_SHOW;

        n->actions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        n->default_action_name = g_strdup("default");

        n->colors_match_urgency = true;

        n->script_count = 0;
        return n;
}

/* see notification.h */
void notification_init(struct notification *n)
{
        /* default to empty string to avoid further NULL faults */
        n->appname  = n->appname  ? n->appname  : g_strdup("unknown");
        n->summary  = n->summary  ? n->summary  : g_strdup("");
        n->body     = n->body     ? n->body     : g_strdup("");
        n->category = n->category ? n->category : g_strdup("");

        /* sanitize urgency */
        if (n->urgency < URG_MIN)
                n->urgency = URG_LOW;
        if (n->urgency > URG_MAX)
                n->urgency = URG_CRIT;

        /* Timeout processing */
        if (n->timeout < 0)
                n->timeout = settings.timeouts[n->urgency];

        /* Color hints */
        struct notification_colors defcolors;
        switch (n->urgency) {
                case URG_LOW:
                        defcolors = settings.colors_low;
                        break;
                case URG_NORM:
                        defcolors = settings.colors_norm;
                        break;
                case URG_CRIT:
                        defcolors = settings.colors_crit;
                        break;
                default:
                        g_error("Unhandled urgency type: %d", n->urgency);
        }

        if (!n->colors.fg)
                n->colors.fg = g_strdup(defcolors.fg);
        if (!n->colors.bg)
                n->colors.bg = g_strdup(defcolors.bg);
        if (!n->colors.highlight)
                n->colors.highlight = g_strdup(defcolors.highlight);
        if (!n->colors.frame)
                n->colors.frame = g_strdup(defcolors.frame);

        /* Sanitize misc hints */
        if (n->progress < 0)
                n->progress = -1;

        /* Process rules */
        rule_apply_all(n);

        if (g_str_has_prefix(n->summary, "DUNST_COMMAND_")) {
                char *msg = "DUNST_COMMAND_* has been removed, please switch to dunstctl. See #830 for more details. https://github.com/dunst-project/dunst/pull/830";
                LOG_W("%s", msg);
                n->body = string_append(n->body, msg, "\n");
        }

        /* Icon handling */
        if (STR_EMPTY(n->iconname))
                g_clear_pointer(&n->iconname, g_free);
        if (!n->icon && !n->iconname && n->default_icon_name) {
                n->iconname = g_strdup(n->default_icon_name);
        }
        if (!n->icon && !n->iconname)
                n->iconname = g_strdup(settings.icons[n->urgency]);


        /* UPDATE derived fields */
        notification_extract_urls(n);
        notification_format_message(n);

        /* Update timeout: dbus_timeout has priority over timeout */
        if (n->dbus_timeout >= 0)
                n->timeout = n->dbus_timeout;

}

static void notification_format_message(struct notification *n)
{
        // Reapply colors based on urgency - this bothered me for quite a long time...
        // When rule changed urgency, it did not change default colors, so my rules had
        // to have multiple overrides that were copied back and forth in config file.
        //TODO: Maybe add notification flag to distinguish that someone overrode one specific
        //      color and that we should not touch colors? <DONE>
        struct notification_colors * new_color = NULL;
        switch (n->urgency) {
                case URG_LOW:
                        new_color = &settings.colors_low;
                        break;
                case URG_NORM:
                        new_color = &settings.colors_norm;
                        break;
                case URG_CRIT:
                        new_color = &settings.colors_crit;
                        break;

                default:
                        // Sigh...
                        LOG_W("Unhandled urgency value: %d (%s)",
                              n->urgency, notification_urgency_to_string(n->urgency));
                        break;
        }
        if ( new_color != NULL && n->colors_match_urgency ) {
                if( n->colors.fg ) g_free( n->colors.fg);
                if( n->colors.bg ) g_free( n->colors.bg);
                if( n->colors.highlight ) g_free( n->colors.highlight);
                if( n->colors.frame ) g_free( n->colors.frame);

                n->colors.fg = g_strdup(new_color->fg);
                n->colors.bg = g_strdup(new_color->bg);
                n->colors.highlight = g_strdup(new_color->highlight);
                n->colors.frame = g_strdup(new_color->frame);
        }


        g_clear_pointer(&n->msg, g_free);

        n->msg = string_replace_all("\\n", "\n", g_strdup(n->format));

        /* replace all formatter */
        for(char *substr = strchr(n->msg, '%');
                  substr && *substr;
                  substr = strchr(substr, '%')) {

                char pg[16];
                char *icon_tmp;

                switch(substr[1]) {
                case 'a':
                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                n->appname,
                                MARKUP_NO);
                        break;
                case 's':
                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                n->summary,
                                MARKUP_NO);
                        break;
                case 'b':
                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                n->body,
                                n->markup);
                        break;
                case 'I':
                        icon_tmp = g_strdup(n->iconname);
                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                icon_tmp ? basename(icon_tmp) : "",
                                MARKUP_NO);
                        g_free(icon_tmp);
                        break;
                case 'i':
                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                n->iconname ? n->iconname : "",
                                MARKUP_NO);
                        break;
                case 'p':
                        if (n->progress != -1)
                                sprintf(pg, "[%3d%%]", n->progress);

                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                n->progress != -1 ? pg : "",
                                MARKUP_NO);
                        break;
                case 'n':
                        if (n->progress != -1)
                                sprintf(pg, "%d", n->progress);

                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                n->progress != -1 ? pg : "",
                                MARKUP_NO);
                        break;
                case '%':
                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                "%",
                                MARKUP_NO);
                        break;
                case '\0':
                        LOG_W("format_string has trailing %% character. "
                              "To escape it use %%%%.");
                        substr++;
                        break;
                default:
                        LOG_W("format_string %%%c is unknown.", substr[1]);
                        // shift substr pointer forward,
                        // as we can't interpret the format string
                        substr++;
                        break;
                }
        }

        n->msg = g_strchomp(n->msg);

        /* truncate overlong messages */
        if (strnlen(n->msg, DUNST_NOTIF_MAX_CHARS + 1) > DUNST_NOTIF_MAX_CHARS) {
                char * buffer = g_strndup(n->msg, DUNST_NOTIF_MAX_CHARS);
                g_free(n->msg);
                n->msg = buffer;
        }
}

static void notification_extract_urls(struct notification *n)
{
        g_clear_pointer(&n->urls, g_free);

        char *urls_in = string_append(g_strdup(n->summary), n->body, " ");

        char *urls_a = NULL;
        char *urls_img = NULL;
        markup_strip_a(&urls_in, &urls_a);
        markup_strip_img(&urls_in, &urls_img);
        // remove links and images first to not confuse
        // plain urls extraction
        char *urls_text = extract_urls(urls_in);

        n->urls = string_append(n->urls, urls_a, "\n");
        n->urls = string_append(n->urls, urls_img, "\n");
        n->urls = string_append(n->urls, urls_text, "\n");

        g_free(urls_in);
        g_free(urls_a);
        g_free(urls_img);
        g_free(urls_text);
}


void notification_update_text_to_render(struct notification *n)
{
        g_clear_pointer(&n->text_to_render, g_free);

        char *buf = NULL;

        char *msg = g_strchomp(n->msg);

        /* print dup_count and msg */
        if ((n->dup_count > 0 && !settings.hide_duplicate_count)
            && (g_hash_table_size(n->actions) || n->urls) && settings.show_indicators) {
                buf = g_strdup_printf("(%d%s%s) %s",
                                      n->dup_count,
                                      g_hash_table_size(n->actions) ? "A" : "",
                                      n->urls ? "U" : "", msg);
        } else if ((g_hash_table_size(n->actions) || n->urls) && settings.show_indicators) {
                buf = g_strdup_printf("(%s%s) %s",
                                      g_hash_table_size(n->actions) ? "A" : "",
                                      n->urls ? "U" : "", msg);
        } else if (n->dup_count > 0 && !settings.hide_duplicate_count) {
                buf = g_strdup_printf("(%d) %s", n->dup_count, msg);
        } else {
                buf = g_strdup(msg);
        }

        /* print age */
        gint64 hours, minutes, seconds;
        gint64 t_delta = time_monotonic_now() - n->timestamp;

        if (settings.show_age_threshold >= 0
            && t_delta >= settings.show_age_threshold) {
                hours   = t_delta / G_USEC_PER_SEC / 3600;
                minutes = t_delta / G_USEC_PER_SEC / 60 % 60;
                seconds = t_delta / G_USEC_PER_SEC % 60;

                char *new_buf;
                if (hours > 0) {
                        new_buf =
                            g_strdup_printf("%s (%ldh %ldm %lds old)", buf, hours,
                                            minutes, seconds);
                } else if (minutes > 0) {
                        new_buf =
                            g_strdup_printf("%s (%ldm %lds old)", buf, minutes,
                                            seconds);
                } else {
                        new_buf = g_strdup_printf("%s (%lds old)", buf, seconds);
                }

                g_free(buf);
                buf = new_buf;
        }

        n->text_to_render = buf;
}

/* see notification.h */
void notification_do_action(struct notification *n)
{
        assert(n->default_action_name);
        
        if (g_hash_table_size(n->actions)) {
                if (g_hash_table_contains(n->actions, n->default_action_name)) {
                        signal_action_invoked(n, n->default_action_name);
                        return;
                }
                if (strcmp(n->default_action_name, "default") == 0 && g_hash_table_size(n->actions) == 1) {
                        GList *keys = g_hash_table_get_keys(n->actions);
                        signal_action_invoked(n, keys->data);
                        g_list_free(keys);
                        return;
                }
                notification_open_context_menu(n);

        }
}

/* see notification.h */
void notification_open_url(struct notification *n)
{
        if (!n->urls) {
                LOG_W("There are no URL's in this notification");
                return;
        }
        if (strstr(n->urls, "\n"))
                notification_open_context_menu(n);
        else
                open_browser(n->urls);
}

/* see notification.h */
void notification_open_context_menu(struct notification *n)
{
        GList *notifications = NULL;
        notifications = g_list_append(notifications, n);
        notification_lock(n);

        context_menu_for(notifications);
}

void notification_invalidate_actions(struct notification *n) {
        g_hash_table_remove_all(n->actions);
}

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */
