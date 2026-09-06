/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Exercise the shipped backend, not a duplicate parser or flag-only facade. */
#include "../third_party/openport2-j2534/j2534.c"
#define REQUIRE(expr) do { if (!(expr)) { fprintf(stderr, "failure: %s at line %d\n", #expr, __LINE__); abort(); } } while (0)

static unsigned char input[PM_DATA_LEN];
static int input_size, input_used, writes, closes, releases, exits;
static int initialization, initialization_reads, fail_initialization_read;
static struct libusb_device_handle handle;
static libusb_device device;
static libusb_device *devices[] = { &device, NULL };
static const struct libusb_endpoint_descriptor endpoints[] = {{2, 0x81}, {2, 2}};
static const struct libusb_interface_descriptor setting = {2, 0, endpoints};
static const struct libusb_interface interface = {1, &setting};
static struct libusb_config_descriptor config = {1, &interface};
int libusb_init(struct libusb_context **p) { *p = NULL; return 0; }
void libusb_exit(struct libusb_context *p) { (void)p; exits++; }
ssize_t libusb_get_device_list(struct libusb_context *p, libusb_device ***d) { (void)p; *d=devices; return 1; }
void libusb_free_device_list(libusb_device **p, int n) { (void)p; (void)n; }
int libusb_get_device_descriptor(libusb_device *p, struct libusb_device_descriptor *d) { (void)p; d->idVendor=0x0403; d->idProduct=0xcc4d; return 0; }
int libusb_open(libusb_device *p, struct libusb_device_handle **h) { (void)p; *h=&handle; return 0; }
void libusb_close(struct libusb_device_handle *p) { (void)p; closes++; }
uint8_t libusb_get_device_address(libusb_device *p) { (void)p; return 1; }
int libusb_get_config_descriptor(libusb_device *p, uint8_t n, struct libusb_config_descriptor **d) { (void)p; (void)n; *d=&config; return 0; }
void libusb_free_config_descriptor(struct libusb_config_descriptor *p) { (void)p; }
int libusb_kernel_driver_active(struct libusb_device_handle *p, int n) { (void)p; (void)n; return 0; }
int libusb_detach_kernel_driver(struct libusb_device_handle *p, int n) { (void)p; (void)n; return 0; }
int libusb_claim_interface(struct libusb_device_handle *p, int n) { (void)p; (void)n; return 0; }
int libusb_release_interface(struct libusb_device_handle *p, int n) { (void)p; (void)n; releases++; return 0; }
const char *libusb_error_name(int n) { (void)n; return "mock USB failure"; }
int libusb_bulk_transfer(struct libusb_device_handle *p, unsigned char ep,
    unsigned char *data, int capacity, int *transferred, unsigned int timeout)
{
    (void)p; (void)timeout; *transferred=0;
    if (!(ep & 0x80)) { writes++; *transferred=capacity; return 0; }
    if (initialization) {
        const char *reply = initialization_reads == 0 ? "ari firmware\r\n" : "aro\r\n";
        if (++initialization_reads == fail_initialization_read) return LIBUSB_ERROR_TIMEOUT;
        REQUIRE((int)strlen(reply) <= capacity);
        memcpy(data, reply, strlen(reply)); *transferred=(int)strlen(reply); return 0;
    }
    if (input_used++) return LIBUSB_ERROR_TIMEOUT;
    REQUIRE(input_size <= capacity);
    memcpy(data, input, (size_t)input_size); *transferred=input_size; return 0;
}
static int read_packet(const unsigned char *bytes, size_t size, PASSTHRU_MSG *msg)
{
    unsigned long count=1;
    REQUIRE(size <= sizeof(input)); memcpy(input, bytes, size);
    input_size=(int)size; input_used=0;
    con->channel=CAN; con->protocol_id=5; endpoint->addr_in=0x81;
    memset(msg, 0, sizeof(*msg));
    return PassThruReadMsgs(5, msg, &count, 1);
}
int main(void)
{
    PASSTHRU_MSG msg;
    unsigned long count, id;
    unsigned char packet[] = {0x61,0x72,0x35,9,0, 0,0,0,1, 0,0,7,0xe8};
    REQUIRE(read_packet(packet, sizeof(packet), &msg) == J2534_NOERROR);
    REQUIRE(msg.DataSize == 4 && msg.Data[3] == 0xe8 && msg.Timestamp == 1);
    for (size_t n=0; n<sizeof(packet); n++)
        REQUIRE(read_packet(packet, n, &msg) == J2534_ERR_INVALID_MSG);
    for (unsigned int n=0; n<256; n++) {
        packet[3]=(unsigned char)n;
        int result=read_packet(packet, sizeof(packet), &msg);
        REQUIRE(n == 9 ? result == J2534_NOERROR : result == J2534_ERR_INVALID_MSG);
    }
    packet[3]=9;
    /* Exact USB-buffer boundary: no speculative read after the final record. */
    unsigned char full[PM_DATA_LEN];
    size_t offset=0;
    for (int i=0; i<29; i++) { memcpy(full+offset, packet, 13); offset+=13; }
    for (int i=0; i<15; i++) {
        memset(full+offset, 0, 250); full[offset]=0x61; full[offset+1]=0x72;
        full[offset+2]=CAN; full[offset+3]=246; offset+=250;
    }
    full[offset++]=0x61; /* Truncated next header at last allocated byte. */
    REQUIRE(offset==sizeof(full));
    REQUIRE(read_packet(full, sizeof(full), &msg)==J2534_ERR_INVALID_MSG);
    REQUIRE(fifo_head==NULL);
    memset(&msg, 0, sizeof(msg)); msg.DataSize=PM_DATA_LEN;
    count=1; writes=0;
    REQUIRE(PassThruWriteMsgs(5, &msg, &count, 1)==J2534_ERR_INVALID_MSG);
    REQUIRE(count==0 && writes==0);
    msg.DataSize=4; count=1;
    REQUIRE(PassThruWriteMsgs(5, &msg, &count, 1)==J2534_NOERROR);
    REQUIRE(count==1 && writes==1);
    REQUIRE(PassThruReadMsgs(5, &msg, NULL, 1)==J2534_ERR_NULL_PARAMETER);
    initialization=1;
    for (int failure=1; failure<=2; failure++) {
        initialization_reads=0; fail_initialization_read=failure;
        closes=releases=exits=0; id=99;
        REQUIRE(PassThruOpen(NULL, &id)==J2534_ERR_TIMEOUT);
        REQUIRE(id==0 && con->dev_handle==NULL && closes==1 && releases==1 && exits==1);
        REQUIRE(LAST_ERROR[0]!='\0');
    }
    puts("OpenPort packet bounds and initialization regression tests passed");
    return 0;
}
