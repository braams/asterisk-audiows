/*
 */

/*! \file
 *
 * \brief AudioWS application -- transmit and receive audio through a WebSocket
 *
 * \author Max Nesterov <braams@braams.ru>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/file.h"
#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/app.h"
#include "asterisk/pbx.h"
#include "asterisk/http_websocket.h"
#include "asterisk/format_cache.h"

/*** DOCUMENTATION
	<application name="AudioWS" language="en_US">
		<synopsis>
			Transmit and receive audio between channel and WebSocket
		</synopsis>
		<syntax>
		<parameter name="url" required="true">
        				<argument name="url" required="true">
        					<para>the URL to the websocket server you want to send the audio to. </para>
        				</argument>
        </parameter>
		</syntax>
		<description>
			<para>Connects to the given Websocket service, then transmits channel audio over that socket.
			 In turn, audio is received from the socket and sent to the channel.
			 Only audio frames will be transmitted.</para>
			<para>This application does not automatically answer and should be
			preceeded by an application such as Answer() or Progress().</para>
		</description>
	</application>
 ***/

static const char app[] = "AudioWS";


static int audiows_exec(struct ast_channel *chan, const char *data) {

    char *parse;
    struct ast_websocket *websocket;
    struct ast_format *readFormat, *writeFormat;
    const char *chanName;
    enum ast_websocket_result result;

    AST_DECLARE_APP_ARGS(args, AST_APP_ARG(url););

    parse = ast_strdupa(data);

    AST_STANDARD_APP_ARGS(args, parse);

    if (ast_strlen_zero(args.url)) {
        ast_log(LOG_ERROR, "AudioWS requires an argument (url)\n");
        return -1;
    }
    ast_verb(2, "Connecting websocket server at %s\n", args.url);

    websocket = ast_websocket_client_create(args.url, "echo", NULL, &result);

    if (result != WS_OK) {
        ast_log(LOG_ERROR, "Could not connect to websocket\n");
        return -1;
    }
    chanName = ast_channel_name(chan);
    char *json_buffer;
    struct ast_json *hello_json;
    struct ast_json *dtmf_json;
    hello_json = ast_json_pack("{s:s, s:s}", "Event", "Hello", "Channel", S_OR(chanName, ""));
    json_buffer = ast_json_dump_string(hello_json);
    ast_json_unref(hello_json);


    if (ast_websocket_write(websocket, AST_WEBSOCKET_OPCODE_TEXT, json_buffer, strlen(json_buffer))) {
        ast_log(LOG_ERROR, "Could not write to websocket.\n");
        return -1;
    }


    writeFormat = ao2_bump(ast_channel_writeformat(chan));
    readFormat = ao2_bump(ast_channel_readformat(chan));

    if (ast_set_write_format(chan, ast_format_slin) || ast_set_read_format(chan, ast_format_slin)) {
        ast_log(LOG_WARNING, "Unable to set '%s' to signed linear format \n", ast_channel_name(chan));
        ao2_ref(writeFormat, -1);
        ao2_ref(readFormat, -1);
        return -1;
    }

    int res = -1;

    char *payload;
    uint64_t payload_len;
    enum ast_websocket_opcode opcode;
    int fragmented;

    while (ast_waitfor(chan, -1) > -1) {
        struct ast_frame *f = ast_read(chan);
        if (!f) {
            break;
        }

        f->delivery.tv_sec = 0;
        f->delivery.tv_usec = 0;
        if (f->frametype == AST_FRAME_VOICE) {


            if (ast_websocket_write(websocket, AST_WEBSOCKET_OPCODE_BINARY, f->data.ptr, f->datalen)) {
                ast_log(LOG_ERROR, "Could not write to websocket\n");
                return -1;
            }

            if (ast_websocket_read(websocket, &payload, &payload_len, &opcode, &fragmented)) {
                ast_log(LOG_WARNING, "WebSocket read error: %s\n", strerror(errno));
                return -1;
            }

            switch (opcode) {
                case AST_WEBSOCKET_OPCODE_CLOSE:
                    ast_log(LOG_ERROR, "WebSocket closed\n");
                    return -1;
                case AST_WEBSOCKET_OPCODE_BINARY:
                    //ast_verb(2, "ws type %ud, len %ld\n", opcode, payload_len);
                    //ast_frame_dump("name",f,"prefix");
                    memcpy(f->data.ptr, payload, payload_len);

                    if (ast_write(chan, f)) {
                        ast_frfree(f);
                        goto end;
                    }
                    break;
                case AST_WEBSOCKET_OPCODE_TEXT:
                    break;
                default:
                    /* Ignore all other message types */
                    break;
            }
        }

        if (f->frametype == AST_FRAME_DTMF) {
            ast_verb(2, "DTMF: %c\n", f->subclass.integer);
            dtmf_json = ast_json_pack("{s:s, s:s}", "Event", "DTMF", "Digit", "2");
            json_buffer = ast_json_dump_string(dtmf_json);

            ast_json_unref(dtmf_json);
            if (ast_websocket_write(websocket, AST_WEBSOCKET_OPCODE_TEXT, json_buffer, strlen(json_buffer))) {
                ast_log(LOG_ERROR, "Could not write to websocket .\n");
            }
        }

        ast_frfree(f);
    }
    end:
    return res;
}

static int unload_module(void) {
    return ast_unregister_application(app);
}

static int load_module(void) {
    return ast_register_application_xml(app, audiows_exec);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "AudioWS Application");