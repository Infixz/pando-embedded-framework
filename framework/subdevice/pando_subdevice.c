#include "../subdevice/pando_subdevice.h"
#include "../gateway/pando_channel.h"
#include "../protocol/common_functions.h"
#include "../protocol/sub_device_protocol.h"
#include "../subdevice/pando_object.h"
#include "../subdevice/pando_event.h"
#include "../subdevice/pando_command.h"
#include "../platform/include/pando_sys.h"

#define CMD_QUERY_STATUS (65528)

static void FUNCTION_ATTRIBUTE
decode_data(struct sub_device_buffer *device_buffer)
{
    struct pando_property data_body;
    uint8_t *buf_end = device_buffer->buffer + device_buffer->buffer_length;
    uint16_t tlv_type, tlv_length;
    uint8_t *value = NULL;

    PARAMS *object_param = NULL;
    while((object_param = get_sub_device_property(device_buffer, &data_body))){
        pando_object* obj = find_pando_object(data_body.property_num);
        if( NULL == obj )
        {
        	pd_printf("object [%d] not found in list\n", data_body.property_num);
        }

        obj->unpack(object_param);
    }
}

static void FUNCTION_ATTRIBUTE
send_current_status()
{
    struct sub_device_buffer* data_buffer;
    data_buffer = create_data_package(0);
    if(NULL == data_buffer)
    {
    	pd_printf("create data package error\n");
        return;
    }

    pando_object* obj = NULL;
    pando_objects_iterator* it = create_pando_objects_iterator();
    while((obj = pando_objects_iterator_next(it))){
        PARAMS* params =  create_params_block();
        if (params == NULL)
        {
        	pd_printf("Create params block failed.\n");
            return;
        }
        obj->pack(params);
        int ret = add_next_property(data_buffer, obj->no, params);

        if (ret != 0)
        {
        	pd_printf("add_next_property failed.");
        }

        delete_params_block(params);
    }
    delete_pando_objects_iterator(it);

    channel_send_to_device(PANDO_CHANNEL_PORT_1, data_buffer->buffer, data_buffer->buffer_length);
    show_package(data_buffer->buffer, data_buffer->buffer_length);
    delete_device_package(data_buffer);
}



static void FUNCTION_ATTRIBUTE
decode_command(struct sub_device_buffer *device_buffer)
{
    struct pando_command cmd_body;
    pd_command *p_cmd_body;
    PARAMS *cmd_param = get_sub_device_command(device_buffer, &cmd_body);
    if(CMD_QUERY_STATUS == cmd_body.command_num)
    {
    	pd_printf("receive a get request\n");
        send_current_status();
    }
    else
    {
        pd_printf("\nReceive a command:%d\n", cmd_body.command_num);
        p_cmd_body = find_pando_command(cmd_body.command_num);
        if(p_cmd_body == NULL)
        {
        	pd_printf("no such command find");
        	return;
        }
        if(p_cmd_body->unpack != NULL)
        {
        	p_cmd_body->unpack(cmd_param);
        }
    }

}

void FUNCTION_ATTRIBUTE
pando_subdevice_recv(uint8_t * buffer, uint16_t length)
{
    if(NULL == buffer)
    {
        return;
    }

    pd_printf("subdevive receive a package: \n");
    show_package(buffer, length);

    struct sub_device_buffer *device_buffer = (struct sub_device_buffer *)pd_malloc(sizeof(struct sub_device_buffer));
    device_buffer->buffer_length = length;
    device_buffer->buffer = (uint8_t *)pd_malloc(length);

    pd_memcpy(device_buffer->buffer, buffer, length);

    uint16_t payload_type = get_sub_device_payloadtype(device_buffer);

    switch (payload_type) {
    case PAYLOAD_TYPE_DATA:
        decode_data(device_buffer);
        break;
    case PAYLOAD_TYPE_COMMAND:
        decode_command(device_buffer);
        break;
    default:
    	pd_printf("unsuported paload type : %d", payload_type);
        break;
    }

    delete_device_package(device_buffer);
}

/******************************************************************************
 * FunctionName : report_event.
 * Description  : report the specify event.
 * Parameters   : no: the event no.
 * Returns      : none.
*******************************************************************************/
void FUNCTION_ATTRIBUTE
report_event(uint8_t no)
{
    struct sub_device_buffer* event_buffer;
	event_buffer = create_event_package(0);
	if(NULL == event_buffer)
	{
		pd_printf("create event package error\n");
		return;
	}

	pd_event* event = find_pando_event(no);
	if(event == NULL)
	{
		pd_printf("no such event find\n");
		return;
	}

	PARAMS* params =  create_params_block();
	if (params == NULL)
	{
		pd_printf("Create params block failed.\n");
		return;
	}

	if(event->pack != NULL)
	{
		event->pack(params);
	}

	int ret =  add_event(event_buffer, no,  event->priority, params);

	if (ret != 0)
	{
		pd_printf("add event failed.");
		return;
	}

	delete_params_block(params);
	show_package(event_buffer->buffer, event_buffer->buffer_length);
	channel_send_to_device(PANDO_CHANNEL_PORT_1, event_buffer->buffer, event_buffer->buffer_length);
	delete_device_package(event_buffer);

}
