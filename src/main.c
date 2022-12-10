// LOGGING
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_http_client_sample, LOG_LEVEL_DBG);
// CLANG
#include <stdio.h>
#include <string.h>
// KERNEL & MISC
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/display/cfb.h>
// NET
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/http/parser.h>

#include <zephyr/display/cfb.h>

static int doRequest(void);

#define MAX_RECV_BUF_LEN 512

static const struct device *display_dev;

static uint8_t font_width;
static uint8_t font_height;

static void initDisplay(const struct device *dev) {
	display_set_pixel_format(dev, PIXEL_FORMAT_MONO10);
	cfb_framebuffer_init(dev);

	cfb_framebuffer_clear(dev, true);
	display_blanking_off(dev);

	cfb_framebuffer_set_font(dev, 1);
	cfb_get_font_size(dev, 1, &font_width, &font_height);
}

static void printDisplay(const struct device *dev, const char *str) 
{
	printf("displaying: %s\n", str);
	cfb_framebuffer_clear(dev, false);
	cfb_print(dev, str, 0, 0);
	cfb_framebuffer_finalize(dev);
}

static bool connected = false;
struct net_mgmt_event_callback wifi_cb;
void net_cb_hdr(struct net_mgmt_event_callback *cb,
			uint32_t mgmt_event,
			struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		printDisplay(display_dev, "Wifi connected");
		connected = true;
	}
}

#define DEBOUNCE_TIMEOUT_MS 50
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);

uint64_t last_time = 0;
static char addr_str[32];
struct gpio_callback button_cb;
static bool flag = false;
void bt_cb_hdr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) 
{
	// debouncing
	uint64_t now = k_uptime_get();
	if ((now - last_time) > DEBOUNCE_TIMEOUT_MS)
	{
		printf("Pushbutton pressed\n");
		flag = true;
	}
	last_time = now;
}

static uint8_t recv_buf_ipv4[MAX_RECV_BUF_LEN];

static void response_cb(struct http_response *rsp,
			enum http_final_call final_data,
			void *user_data)
{
	printf("\tgot response\n");
	if (final_data == HTTP_DATA_MORE) {
		LOG_INF("Partial data received (%zd bytes)", rsp->data_len);
	} else if (final_data == HTTP_DATA_FINAL) {
		LOG_INF("All the data received (%zd bytes)", rsp->data_len);
	}

	LOG_INF("Response to %s", (const char *)user_data);
	LOG_INF("Response status %s", rsp->http_status);
}

static int doRequest(void)
{
	int fd_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	printf("\tsocket\n");
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(80);
	zsock_inet_pton(AF_INET, addr_str, &addr.sin_addr);

	int ret = zsock_connect(fd_socket, (struct sockaddr *)&addr, sizeof(addr));
	printf("\tconnect\n");

	if (ret < 0) {
		LOG_ERR("Cannot connect to remote (%d)", -errno);
	}

	struct http_request req;
	memset(&req, 0, sizeof(req));

	req.method = HTTP_GET;
	req.url = "/";
	req.host = addr_str;
	req.protocol = "HTTP/1.1";
	req.response = response_cb;
	req.recv_buf = recv_buf_ipv4;
	req.recv_buf_len = sizeof(recv_buf_ipv4);

	printf("\tsending request\n");
	ret = http_client_req(fd_socket, &req, 3000, "IPv4 GET");
	
	close(fd_socket);
	printf("\tclose\n");

	return ret;
}

void main(void)
{
    // Init Display
	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	initDisplay(display_dev);

    // Init WIFI Connection
	static struct net_if *wifi_if;
	wifi_if = net_if_get_default();
	static const struct device *wifi_dev;
	wifi_dev = net_if_get_device(wifi_if);

	net_mgmt_init_event_callback(&wifi_cb, net_cb_hdr, NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	printDisplay(display_dev, "connecting");

	struct wifi_connect_req_params params;
	params.ssid = "NOTEBOOK-MINWOO";
	params.ssid_length = 15;
	params.psk = "01087655202";
	params.psk_length = 11;
	params.security = WIFI_SECURITY_TYPE_PSK;

	((struct net_wifi_mgmt_offload *)(wifi_dev->api))->connect(wifi_dev, &params);

	while (!connected) k_msleep(500);

	struct net_if_ipv4 * cfg;
	net_if_config_ipv4_get(wifi_if, &cfg);
	
	inet_ntop(AF_INET, &(cfg->gw), addr_str, sizeof(addr_str));
	printf("\tDHCP Done. Destination is : %s\n", addr_str);
	printDisplay(display_dev, addr_str);

	// Init Pushbutton
	gpio_pin_configure_dt(&button, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);

	gpio_init_callback(&button_cb, bt_cb_hdr, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb);
	
	// Loop
	while (1) {
		if (flag) {
			
			if (doRequest() >= 0) {
				cfb_framebuffer_clear(display_dev, false);

				int row = 0;
				int start = 0;
				bool body = false;
				for (int i=0; i < MAX_RECV_BUF_LEN; i++) {
					if (recv_buf_ipv4[i] == '\n' || recv_buf_ipv4[i] == '\0') {
						int len = i - start;
						if (body) {
							char* tmp = k_malloc(len + 1);
							memcpy(tmp, &recv_buf_ipv4[start], len);
							tmp[len] = '\0';
							printf("%s\n",tmp);
							cfb_print(display_dev, tmp, 0, font_height * row++);
							k_free(tmp);
						}
						if (recv_buf_ipv4[i] == '\0') break;
						if (len == 1) body = true;
						start = i + 1;
					}
				}

				if (body) cfb_framebuffer_finalize(display_dev);

			}

			

			flag = false;
		}

		k_cpu_idle();

	}
}