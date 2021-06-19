/**
 * bluetooth.c
 *   1. Determines a service channel
 *
 *   @ref
 *     https://people.csail.mit.edu/albert/bluez-intro/x604.html
 *     https://people.csail.mit.edu/albert/bluez-intro/x502.html
 *   accessed
 *     18 June 2021
 */

#include <stdio.h>
#include <string.h>
//#include <unistd.h>
#include <stdlib.h>

//#include <sys/socket.h>

#include <bluetooth/sdp.h>
//#include <bluetooth/rfcomm.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp_lib.h>

#define UUID uuid_t
#define BYTE uint8_t
#define LIST sdp_list_t
#define DATA sdp_data_t
#define RECORD sdp_record_t
#define MAC_ADDRESS bdaddr_t
#define SESSION sdp_session_t

#define DEVICE_ID "C0:8C:71:61:34:8C" // android device

int getServiceChannel(BYTE* uuid)
{
    /*  connect to sdp server: device must be ON but server need not be running
     */
    BYTE address[6];
    str2ba(DEVICE_ID, (MAC_ADDRESS*)&address);
    SESSION* session
        = sdp_connect(BDADDR_ANY, (MAC_ADDRESS*)&address, SDP_RETRY_IF_BUSY);
    if (!session) {
        fprintf(stderr, "-- can't connect to sdp server\n");
        return (0);
    }

    /* create query lists */
    UUID deviceUuid;
    int range = 0x0000ffff; // from 0000 to ffff
    sdp_uuid128_create(&deviceUuid, uuid);
    LIST* responseList;
    LIST* searchList = sdp_list_append(NULL, &deviceUuid);
    LIST* attrIdList = sdp_list_append(NULL, &range);

    /* search for records: server must be running */
    int success = sdp_service_search_attr_req(
        session, searchList, SDP_ATTR_REQ_RANGE, attrIdList, &responseList);
    if (success) {
        fprintf(stderr, "-- search failed\n");
        return (0);
    }

    /* check responses */
    success = sdp_list_len(responseList);
    if (success <= 0) {
        fprintf(stderr, "-- no responses\n");
        return (0);
    }

    /* process responses */
    int channel = 0;
    LIST* responses = responseList;
    while (responses) {
        LIST *protoList, *protocol;
        RECORD* record = (RECORD*)responses->data;
        success = sdp_get_access_protos(record, &protoList);
        if (success) {
            fprintf(stderr, "-- can't get access protocols\n");
            return (0);
        }

        protocol = protoList;
        while (protocol) {
            LIST* pds;
            int protocolCount = 0;
            pds = (LIST*)protocol->data;

            while (pds) { // loop thru all pds
                DATA* d;
                int dtd;
                d = pds->data;
                while (d) {
                    dtd = d->dtd;
                    switch (dtd) {
                    case SDP_UUID16:
                    case SDP_UUID32:
                    case SDP_UUID128:
                        protocolCount = sdp_uuid_to_proto(&d->val.uuid);
                        break;
                    case SDP_UINT8:
                        if (protocolCount == RFCOMM_UUID) {
                            channel = d->val.uint8; // save channel id
                        }
                        break;
                    default:
                        break;
                    }
                    d = d->next; // to next data unit
                }
                pds = pds->next; // to next pds
            }
            sdp_list_free((LIST*)protocol->data, 0);

            protocol = protocol->next; // to next protocol
        }
        sdp_list_free(protoList, 0);

        responses = responses->next; // to next response
    }
    /* channel in range [ 0 - 30 ] ( 0 if not found ) */
    return (channel);
}

int main()
{
    /* shared UUID: d8308c4e-9469-4051-8adc-7a2663e415e2 */
    static BYTE DEVICE_UUID[16] = { 0xd8, 0x30, 0x8c, 0x4e, 0x94, 0x69, 0x40,
        0x51, 0x8a, 0xdc, 0x7a, 0x26, 0x63, 0xe4, 0x15, 0xe2 };

    int channel = getServiceChannel(DEVICE_UUID);

    if (channel > 0) {
        printf("-- rfcomm found on channel %d\n", channel);
    } else {
        fprintf(stderr, "-- no rfcomm channel found\n");
        exit(0);
    }

    return 0;
}