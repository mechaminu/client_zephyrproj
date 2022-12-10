from http.server import BaseHTTPRequestHandler, HTTPServer
import requests
import json

wc = dict([
    (0,'clear'),
    (1,'cloudy'),
    (2,'cloudy'),
    (3,'cloudy'),
    (45,'fog'),
    (48,'fog'),
    (51,'rain'),
    (53,'rain'),
    (55,'rain'),
    (56,'snow'),
    (57,'snow'),
    (61,'rain'),
    (63,'rain'),
    (65,'rain'),
    (66,'snow'),
    (67,'snow'),
    (71,'snow'),
    (73,'snow'),
    (75,'snow'),
    (77,'snow'),
    (80,'rain'),
    (81,'rain'),
    (82,'rain'),
    (85,'snow'),
    (86,'snow')
])

res = requests.get("https://api.open-meteo.com/v1/forecast?latitude=37.29&longitude=127.01&current_weather=true&timezone=Asia%2FTokyo")
if res.status_code == 200:
    data = json.loads(res.text)
data = data["current_weather"]
data_str = wc[data["weathercode"]] + "\n" + str(data["temperature"]) + "'C"

class MyHttpHandler(BaseHTTPRequestHandler):
    def print_http_request_detail(self):
        """Print HTTP request in detail."""
        print('::Client address   : {0}'.format(self.client_address[0]))
        print('::Client port      : {0}'.format(self.client_address[1]))
        print('::Request command  : {0}'.format(self.command))
        print('::Request line     : {0}'.format(self.requestline))
        print('::Request path     : {0}'.format(self.path))
        print('::Request version  : {0}'.format(self.request_version))

    def send_http_response_header(self):
        """Create and send http Response."""
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()

    def do_GET(self):
        """HTTP GET request handler."""
        print("## do_GET() activated.")

        # GET response header generation
        self.print_http_request_detail()
        self.send_http_response_header()

        # GET response generation
        self.wfile.write(bytes(data_str, "utf-8"))
        print("## GET request for directory => {0}.".format(self.path))

    def log_message(self, format, *args):
        """Turn off default http.server log message."""
        return

    def simple_calc(self, para1, para2):
        """Multiplication function."""
        return para1 * para2

    def parameter_retrieval(self, msg):
        """Parameter retrieval function for multiplication."""
        result = []
        fields = msg.split('&')
        result.append(int(fields[0].split('=')[1]))
        result.append(int(fields[1].split('=')[1]))
        return result

if __name__ == "__main__": 
    """Main function."""
    server_name = "0.0.0.0"
    server_port = 80

    webServer = HTTPServer((server_name, server_port), MyHttpHandler)
    print("## HTTP server started at http://{0}:{1}.".format(server_name, server_port))

    try:
        webServer.serve_forever()
    except KeyboardInterrupt:
        pass

    webServer.server_close()
    print("HTTP server stopped.")