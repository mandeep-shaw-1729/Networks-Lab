import os
import base64
import subprocess

def get_base64_image(filename):
    if not os.path.exists(filename):
        print(f"Warning: {filename} does not exist!")
        return ""
    with open(filename, "rb") as f:
        data = f.read()
    b64 = base64.b64encode(data).decode("utf-8")
    return f"data:image/png;base64,{b64}"

img_q1_s1 = get_base64_image("Q1-s1.png")
img_q1_s2 = get_base64_image("Q1-s2.png")
img_q2_2a = get_base64_image("Q2-2a.png")
img_q2_rtt = get_base64_image("Screenshot_2026-11-06_12-19-49.png")
img_q2_host = get_base64_image("Screenshot_2026-11-06_12-22-39.png")
img_filt_http = get_base64_image("Screenshot_2026-11-06_12-25-53.png")
img_filt_tcp = get_base64_image("Screenshot_2026-11-06_12-27-01.png")
img_filt_dns = get_base64_image("Screenshot_2026-11-06_12-27-46.png")
img_filt_udp = get_base64_image("Screenshot_2026-11-06_12-28-50.png")
img_follow = get_base64_image("Screenshot_2026-11-06_12-34-19.png")
img_q6_eth = get_base64_image("Screenshot_2026-11-06_12-37-24.png")
img_q6_bits = get_base64_image("Screenshot_2026-11-06_12-38-51.png")
img_q8_pc = get_base64_image("Screenshot_2026-11-06_12-41-28.png")
img_q8_gw = get_base64_image("Screenshot_2026-11-06_12-42-35.png")
img_q9_tcp = get_base64_image("Screenshot_2026-11-06_12-44-57.png")
img_q9_udp = get_base64_image("Screenshot_2026-11-06_12-44-24.png")
img_q10_gen = get_base64_image("Screenshot_2026-11-06_12-47-01.png")
img_q10_tcp = get_base64_image("Screenshot_2026-11-06_12-47-46.png")

html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Assignment 5: Packet Tracer and Traffic Analysis with Wireshark</title>
<style>
    body {{
        font-family: Calibri, "Segoe UI", Arial, sans-serif;
        font-size: 11pt;
        line-height: 1.5;
        color: #2d3748;
        margin: 20px;
    }}
    .title-box {{
        text-align: center;
        border-bottom: 2px solid #1a365d;
        padding-bottom: 15px;
        margin-bottom: 25px;
    }}
    .title-box h1 {{
        color: #1a365d;
        font-size: 22pt;
        margin: 0 0 5px 0;
        text-transform: uppercase;
        letter-spacing: 1px;
    }}
    .title-box h2 {{
        color: #2b6cb0;
        font-size: 14pt;
        margin: 0 0 10px 0;
        font-weight: 600;
    }}
    .meta-table {{
        margin: 15px auto;
        width: 80%;
        border-collapse: collapse;
    }}
    .meta-table td {{
        padding: 5px 12px;
        font-size: 10.5pt;
        border: none;
    }}
    .meta-label {{
        font-weight: bold;
        color: #2d3748;
        width: 35%;
    }}
    h1 {{
        color: #1a365d;
        font-size: 16pt;
        border-bottom: 1.5px solid #cbd5e0;
        padding-bottom: 5px;
        margin-top: 30px;
        page-break-after: avoid;
    }}
    h2 {{
        color: #2b6cb0;
        font-size: 13pt;
        margin-top: 20px;
        margin-bottom: 8px;
        page-break-after: avoid;
    }}
    h3 {{
        color: #2d3748;
        font-size: 11.5pt;
        margin-top: 15px;
        margin-bottom: 6px;
        page-break-after: avoid;
    }}
    p {{
        margin: 0 0 10px 0;
        text-align: justify;
    }}
    ul, ol {{
        margin: 0 0 12px 25px;
        padding: 0;
    }}
    li {{
        margin-bottom: 4px;
    }}
    blockquote {{
        background-color: #f7fafc;
        border-left: 4px solid #3182ce;
        margin: 12px 0;
        padding: 10px 15px;
        font-style: italic;
        color: #4a5568;
    }}
    table.data-table {{
        width: 100%;
        border-collapse: collapse;
        margin: 15px 0;
        font-size: 10pt;
    }}
    table.data-table th {{
        background-color: #2b6cb0;
        color: #ffffff;
        font-weight: bold;
        text-align: left;
        padding: 8px 10px;
        border: 1px solid #cbd5e0;
    }}
    table.data-table td {{
        padding: 7px 10px;
        border: 1px solid #e2e8f0;
        vertical-align: top;
    }}
    table.data-table tr:nth-child(even) {{
        background-color: #f7fafc;
    }}
    .figure-container {{
        text-align: center;
        margin: 18px 0;
        page-break-inside: avoid;
    }}
    .figure-container img {{
        max-width: 98%;
        height: auto;
        border: 1px solid #cbd5e0;
        border-radius: 4px;
        box-shadow: 0 1px 3px rgba(0,0,0,0.1);
    }}
    .caption {{
        font-size: 9.5pt;
        color: #4a5568;
        font-weight: bold;
        margin-top: 6px;
    }}
    code, pre {{
        font-family: Consolas, "Courier New", monospace;
        font-size: 9.5pt;
    }}
    code {{
        background-color: #edf2f7;
        padding: 2px 4px;
        border-radius: 3px;
        color: #805ad5;
    }}
    pre {{
        background-color: #f7fafc;
        border: 1px solid #e2e8f0;
        padding: 10px 14px;
        border-radius: 4px;
        overflow-x: auto;
        margin: 10px 0;
        line-height: 1.4;
    }}
    .highlight-box {{
        background-color: #ebf8ff;
        border: 1px solid #bee3f8;
        border-radius: 4px;
        padding: 12px 15px;
        margin: 12px 0;
    }}
    .page-break {{
        page-break-before: always;
    }}
</style>
</head>
<body>

<div class="title-box">
    <h1>Jadavpur University</h1>
    <h2>Department of Computer Science and Engineering</h2>
    <h2 style="color: #4a5568; font-size: 13pt; font-weight: normal;">Computer Networks Laboratory (BCSE-III)</h2>
    <hr style="border: none; border-top: 1.5px solid #1a365d; margin: 15px 0;">
    <h2 style="color: #1a365d; font-size: 18pt; margin: 10px 0;">Assignment 5: Packet Tracer and Traffic Analysis with Wireshark</h2>
    <p style="font-style: italic; color: #718096; margin: 0 0 15px 0;">Live Packet Capture, Protocol Stack Dissection, ARP Resolution, ICMP Diagnostics, HTTP/TCP Session Flow, Ethernet OUI Identification, and Hierarchy Profiling</p>
    
    <table class="meta-table">
        <tr>
            <td class="meta-label">Student Name:</td>
            <td>Mandeep Shaw</td>
            <td class="meta-label">Roll Number:</td>
            <td>302510501002</td>
        </tr>
        <tr>
            <td class="meta-label">Section:</td>
            <td>A1</td>
            <td class="meta-label">Course:</td>
            <td>BCSE-III Computer Networks Lab</td>
        </tr>
        <tr>
            <td class="meta-label">Date of Submission:</td>
            <td>November 2026</td>
            <td class="meta-label">Tools Used:</td>
            <td>Wireshark v4.2.2, Dumpcap, ping, Firefox</td>
        </tr>
    </table>
</div>

<!-- SECTION 1 -->
<h1>1. Overview and Objectives</h1>
<p>
Network protocol analysis and deep packet inspection (DPI) are fundamental techniques for network diagnostic debugging, performance profiling, cybersecurity audits, and empirical verification of protocol specifications. <strong>Wireshark</strong> is an industry-standard cross-platform packet capture and network protocol analysis suite built on top of the <code>libpcap</code> and <code>dumpcap</code> capture architectures.
</p>
<p>Wireshark partitions captured network traffic into three interconnected interactive inspection panes:</p>
<ol>
    <li><strong>Packet List Panel:</strong> Displays a chronological summary of each captured frame with columns for packet index number, relative or absolute timestamp, source and destination network/hardware addresses, transport/application protocol identification, length in octets, and protocol-specific summary metadata.</li>
    <li><strong>Packet Details Panel:</strong> Presents an expandable hierarchical tree dissecting the protocol stack layer-by-layer (Frame &rarr; Ethernet II &rarr; IPv4/IPv6 &rarr; TCP/UDP/ICMP &rarr; Application layer protocols such as HTTP, DNS, or mDNS).</li>
    <li><strong>Packet Bytes Panel:</strong> Displays the raw byte buffer of the selected packet in traditional hex dump format (16 octets per row in hexadecimal) alongside corresponding ASCII character encodings, with bidirectional selection synchronization linked to the details tree.</li>
</ol>

<div class="page-break"></div>

<!-- QUESTION 1 -->
<h1>2. Question 1: ICMP and ARP Traffic Analysis</h1>
<blockquote>
"Generate some ICMP traffic by using the Ping command line tool to check the connectivity of a neighbouring machine (or router). Note the results in Wireshark. The initial ARP request broadcast from your PC determines the physical MAC address of the network IP Address, and the ARP reply from the neighboring system. After the ARP request, the pings (ICMP echo request and replies) can be seen."
</blockquote>

<h2>2.1 Experimental Methodology</h2>
<p>
To examine the interaction between link-layer address resolution and network-layer reachability testing:
</p>
<ol>
    <li>The local machine was configured on the local subnet with IP address <code>10.223.117.129</code> and MAC address <code>ce:7e:13:24:96:68</code>.</li>
    <li>A live packet capture was initiated in Wireshark on the active network interface.</li>
    <li>The <code>ping</code> utility was executed from the terminal targeting a neighbouring machine at IP <code>10.223.117.59</code>:
        <pre>$ ping -c 4 10.223.117.59</pre>
    </li>
    <li>Wireshark display filters <code>arp</code> and <code>icmp</code> were applied respectively to isolate address resolution and ICMP signaling.</li>
</ol>

<h2>2.2 Address Resolution Protocol (ARP) Analysis</h2>
<p>
Before an IP datagram can be transmitted over a local Ethernet/802.3 broadcast domain, the host's operating system consults its internal ARP cache. Because no entry initially exists for target IP <code>10.223.117.59</code>, an ARP Request frame is broadcast to the Layer-2 broadcast address <code>ff:ff:ff:ff:ff:ff</code>.
</p>

<div class="figure-container">
    <img src="{img_q1_s1}" alt="ARP Packet Capture in Wireshark">
    <div class="caption">Figure 1: Wireshark packet capture filtered by 'arp', capturing ARP request broadcasts and corresponding unicast replies.</div>
</div>

<p><strong>Dissection of Address Resolution:</strong></p>
<ul>
    <li><strong>ARP Request (Packet No. 5228):</strong>
        <ul>
            <li><strong>Source MAC:</strong> <code>ce:7e:13:24:96:68</code> (Transmitting PC)</li>
            <li><strong>Destination MAC:</strong> <code>ff:ff:ff:ff:ff:ff</code> (Ethernet Broadcast)</li>
            <li><strong>Sender IP:</strong> <code>10.223.117.129</code> &rarr; <strong>Target IP:</strong> <code>10.223.117.59</code></li>
            <li><strong>Info:</strong> <code>Who has 10.223.117.59? Tell 10.223.117.129</code></li>
        </ul>
    </li>
    <li><strong>ARP Reply (Packet No. 5229):</strong>
        <ul>
            <li><strong>Source MAC:</strong> <code>46:73:bf:37:e8:7f</code> (Neighbouring Target System)</li>
            <li><strong>Destination MAC:</strong> <code>ce:7e:13:24:96:68</code> (Unicast reply to transmitting PC)</li>
            <li><strong>Sender IP:</strong> <code>10.223.117.59</code> &rarr; <strong>Target IP:</strong> <code>10.223.117.129</code></li>
            <li><strong>Info:</strong> <code>10.223.117.59 is at 46:73:bf:37:e8:7f</code></li>
        </ul>
    </li>
</ul>

<h2>2.3 Internet Control Message Protocol (ICMP) Echo Sequence</h2>
<p>
With the destination MAC address cached, the diagnostic ping packets are transmitted. The display filter <code>icmp</code> captures four consecutive Echo Request and Echo Reply pairs.
</p>

<div class="figure-container">
    <img src="{img_q1_s2}" alt="ICMP Echo Request and Reply Capture">
    <div class="caption">Figure 2: Wireshark packet capture filtered by 'icmp', showing alternating ICMP Echo Requests (Type 8) and Echo Replies (Type 0).</div>
</div>

<table class="data-table">
    <thead>
        <tr>
            <th>No.</th>
            <th>Time (s)</th>
            <th>Source IP</th>
            <th>Destination IP</th>
            <th>Type / Code</th>
            <th>Len</th>
            <th>ICMP Details</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td><strong>5365</strong></td>
            <td>350.365409</td>
            <td>10.223.117.129</td>
            <td>10.223.117.59</td>
            <td>Type 8, Code 0</td>
            <td>98</td>
            <td>Echo request, id=0x0e6f, seq=1/256, ttl=64</td>
        </tr>
        <tr>
            <td><strong>5366</strong></td>
            <td>350.366926</td>
            <td>10.223.117.59</td>
            <td>10.223.117.129</td>
            <td>Type 0, Code 0</td>
            <td>98</td>
            <td>Echo reply, id=0x0e6f, seq=1/256, ttl=64</td>
        </tr>
        <tr>
            <td><strong>5378</strong></td>
            <td>351.367221</td>
            <td>10.223.117.129</td>
            <td>10.223.117.59</td>
            <td>Type 8, Code 0</td>
            <td>98</td>
            <td>Echo request, id=0x0e6f, seq=2/512, ttl=64</td>
        </tr>
        <tr>
            <td><strong>5379</strong></td>
            <td>351.368689</td>
            <td>10.223.117.59</td>
            <td>10.223.117.129</td>
            <td>Type 0, Code 0</td>
            <td>98</td>
            <td>Echo reply, id=0x0e6f, seq=2/512, ttl=64</td>
        </tr>
        <tr>
            <td><strong>5382</strong></td>
            <td>352.368949</td>
            <td>10.223.117.129</td>
            <td>10.223.117.59</td>
            <td>Type 8, Code 0</td>
            <td>98</td>
            <td>Echo request, id=0x0e6f, seq=3/768, ttl=64</td>
        </tr>
        <tr>
            <td><strong>5383</strong></td>
            <td>352.370550</td>
            <td>10.223.117.59</td>
            <td>10.223.117.129</td>
            <td>Type 0, Code 0</td>
            <td>98</td>
            <td>Echo reply, id=0x0e6f, seq=3/768, ttl=64</td>
        </tr>
        <tr>
            <td><strong>5393</strong></td>
            <td>353.370283</td>
            <td>10.223.117.129</td>
            <td>10.223.117.59</td>
            <td>Type 8, Code 0</td>
            <td>98</td>
            <td>Echo request, id=0x0e6f, seq=4/1024, ttl=64</td>
        </tr>
        <tr>
            <td><strong>5394</strong></td>
            <td>353.371728</td>
            <td>10.223.117.59</td>
            <td>10.223.117.129</td>
            <td>Type 0, Code 0</td>
            <td>98</td>
            <td>Echo reply, id=0x0e6f, seq=4/1024, ttl=64</td>
        </tr>
    </tbody>
</table>

<div class="highlight-box">
    <strong>Measured Round-Trip Time (RTT):</strong><br>
    For sequence 1 (Packets 5365 and 5366):<br>
    <code>RTT = T_reply - T_request = 350.366926495 s - 350.365409707 s = 0.001516788 s &asymp; 1.517 ms</code><br>
    The identifier <code>0x0e6f</code> (decimal 3695) uniquely identifies the Linux <code>ping</code> process instance.
</div>

<div class="page-break"></div>

<!-- QUESTION 2 -->
<h1>3. Question 2: Web Traffic Analysis</h1>
<blockquote>
"Generate some web traffic and:<br>
a. Find the list of different protocols that appear in the protocol column in the unfiltered packet-listing window.<br>
b. How long did it take from when the HTTP GET message was sent until the HTTP OK reply was received? (in Time-of-day format).<br>
c. What is the Internet address of the website? What is the Internet address of your computer?<br>
d. Search back through your capture, and find an HTTP packet containing a GET command. Click on the packet in the Packet List Panel. Then expand the HTTP layer in the Packet Details Panel.<br>
e. Find out the value of the Host from the Packet Details Panel, within the GET command."
</blockquote>

<h2>3.1 Part 2a: Protocol Diversity in Unfiltered Packet Listing</h2>
<p>
A web browser was launched to generate HTTP/HTTPS traffic. The unfiltered packet listing captures multiple interleaved protocols across the TCP/IP stack.
</p>

<div class="figure-container">
    <img src="{img_q2_2a}" alt="Unfiltered Packet Listing">
    <div class="caption">Figure 3: Unfiltered packet capture displaying concurrent interleaved protocols during web browsing.</div>
</div>

<p><strong>Observed Protocols in the Protocol Column:</strong></p>
<ol>
    <li><strong>TCP (Transmission Control Protocol):</strong> Manages reliable connection setup (SYN, SYN-ACK, ACK), payload delivery, and connection teardown (FIN, RST).</li>
    <li><strong>TLSv1 / TLSv1.2 (Transport Layer Security):</strong> Encrypted session establishment, cipher suite negotiation, and protected application records.</li>
    <li><strong>HTTP (Hypertext Transfer Protocol):</strong> Cleartext web requests (e.g., <code>GET /js/theme.js HTTP/1.1</code>) and responses.</li>
    <li><strong>DNS (Domain Name System):</strong> Name resolution queries and responses over UDP port 53.</li>
    <li><strong>QUIC (Quick UDP Internet Connections):</strong> Modern multiplexed transport protocol operating on top of UDP port 443 (HTTP/3).</li>
    <li><strong>ICMP (Internet Control Message Protocol):</strong> Error signaling such as <code>Destination unreachable (Port unreachable)</code>.</li>
</ol>

<h2>3.2 Parts 2b & 2c: HTTP Response Time (RTT) and IP Addresses</h2>
<p>
To measure the response time, the time display was switched to <strong>Time-of-day</strong> format via <code>View &rarr; Time Display Format &rarr; Time-of-day</code>, and the display filter <code>http</code> was applied.
</p>

<div class="figure-container">
    <img src="{img_q2_rtt}" alt="HTTP Request and Response in Time-of-Day Format">
    <div class="caption">Figure 4: HTTP GET request (Packet 219) and HTTP 204 OK response (Packet 224) displayed in Time-of-day format.</div>
</div>

<p><strong>Extracted Packet Timestamps:</strong></p>
<ul>
    <li><strong>HTTP GET Request (Packet No. 219):</strong>
        <ul>
            <li>Timestamp: <code>12:16:25.265379115</code></li>
            <li>Source IP (Client Machine): <code>172.28.9.68</code></li>
            <li>Destination IP (Web Server): <code>151.101.193.91</code></li>
            <li>Request Line: <code>GET /generate_204 HTTP/1.1</code></li>
        </ul>
    </li>
    <li><strong>HTTP OK Reply (Packet No. 224):</strong>
        <ul>
            <li>Timestamp: <code>12:16:25.304837622</code></li>
            <li>Source IP (Web Server): <code>151.101.193.91</code></li>
            <li>Destination IP (Client Machine): <code>172.28.9.68</code></li>
            <li>Status: <code>HTTP/1.1 204 OK</code></li>
        </ul>
    </li>
</ul>

<div class="highlight-box">
    <strong>Response Time Calculation:</strong><br>
    <code>&Delta;T = 12:16:25.304837622 - 12:16:25.265379115 = 0.039458507 seconds &asymp; 39.46 ms</code><br><br>
    <strong>Identified Internet Addresses:</strong><br>
    &bull; <strong>Internet Address of Website (Server):</strong> <code>151.101.193.91</code> (Fastly CDN edge node)<br>
    &bull; <strong>Internet Address of Computer (Client):</strong> <code>172.28.9.68</code> (Local private IPv4 address on <code>wlp1s0</code>)
</div>

<h2>3.3 Parts 2d & 2e: Dissecting HTTP GET and Host Parameter</h2>
<p>
Selecting Packet 219 in the Packet List Panel and expanding the <strong>Hypertext Transfer Protocol</strong> layer in the Packet Details Panel reveals the HTTP headers.
</p>

<div class="figure-container">
    <img src="{img_q2_host}" alt="Expanded HTTP Layer Details">
    <div class="caption">Figure 5: Expanded HTTP layer in Details Panel for Packet 219, highlighting the Host parameter and raw bytes.</div>
</div>

<p><strong>Header Parameters:</strong></p>
<ul>
    <li><strong>Request Method:</strong> <code>GET</code></li>
    <li><strong>Request URI:</strong> <code>/generate_204</code></li>
    <li><strong>Request Version:</strong> <code>HTTP/1.1</code></li>
    <li><strong>Host Header:</strong> <code>firefox-portal-detection.com</code></li>
    <li><strong>Full Header Line:</strong> <code>Host: firefox-portal-detection.com\r\n</code></li>
    <li><strong>User-Agent:</strong> <code>Mozilla/5.0 (X11; Linux x86_64; rv:159.0) Gecko/20100101 Firefox/159.0</code></li>
    <li><strong>Connection:</strong> <code>keep-alive</code></li>
</ul>

<div class="highlight-box">
    <strong>Answer for 2e:</strong> The value of the Host parameter from the Packet Details Panel is <strong><code>firefox-portal-detection.com</code></strong>.
</div>

<div class="page-break"></div>

<!-- QUESTION 3 & 4 -->
<h1>4. Question 3: Hex and ASCII Byte Representations</h1>
<blockquote>
"Highlight the Hex and ASCII representations of the packet in the Packet Bytes Panel."
</blockquote>
<p>
When <code>Host: firefox-portal-detection.com\r\n</code> is clicked in the middle Details Panel, Wireshark dynamically highlights the corresponding raw slice in the <strong>Packet Bytes Panel</strong> (offsets <code>0x005e</code> to <code>0x0080</code>), as shown in Figure 5 above.
</p>
<ul>
    <li><strong>Highlighted Hexadecimal Stream:</strong>
        <pre>48 6f 73 74 3a 20 66 69 72 65 66 6f 78 2d 70 6f 72 74 61 6c 2d 64 65 74 65 63 74 69 6f 6e 2e 63 6f 6d 0d 0a</pre>
    </li>
    <li><strong>Highlighted ASCII Stream:</strong>
        <pre>Host: firefox-portal-detection.com..</pre>
    </li>
</ul>
<p>
The trailing <code>..</code> in the ASCII pane corresponds to non-printable control octets <code>0x0d</code> (Carriage Return, <code>\r</code>) and <code>0x0a</code> (Line Feed, <code>\n</code>), terminating the HTTP header per RFC 2616.
</p>

<h1>5. Question 4: First 4 Bytes Hex Value of Host Parameter</h1>
<blockquote>
"Find out the first 4 bytes of the Hex value of the Host parameter from the Packet Bytes Panel."
</blockquote>
<p>Examining the first four highlighted bytes of the Host parameter:</p>

<table class="data-table" style="width: 75%; margin: 15px auto;">
    <thead>
        <tr>
            <th>Byte Position</th>
            <th>Hexadecimal</th>
            <th>Binary</th>
            <th>Decimal</th>
            <th>ASCII Character</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td><strong>1st Byte</strong></td>
            <td><code>48</code></td>
            <td>01001000</td>
            <td>72</td>
            <td><strong>'H'</strong></td>
        </tr>
        <tr>
            <td><strong>2nd Byte</strong></td>
            <td><code>6f</code></td>
            <td>01101111</td>
            <td>111</td>
            <td><strong>'o'</strong></td>
        </tr>
        <tr>
            <td><strong>3rd Byte</strong></td>
            <td><code>73</code></td>
            <td>01110011</td>
            <td>115</td>
            <td><strong>'s'</strong></td>
        </tr>
        <tr>
            <td><strong>4th Byte</strong></td>
            <td><code>74</code></td>
            <td>01110100</td>
            <td>116</td>
            <td><strong>'t'</strong></td>
        </tr>
    </tbody>
</table>

<div class="highlight-box">
    <strong>Answer:</strong> The first 4 bytes of the Hex value of the Host parameter are <strong><code>48 6f 73 74</code></strong>, which directly decodes to the ASCII word <strong><code>Host</code></strong>.
</div>

<div class="page-break"></div>

<!-- QUESTION 5 -->
<h1>6. Question 5: Protocol Filtering and Following Network Flows</h1>
<blockquote>
"Filter packets with http, TCP, DNS and other protocols.<br>
a. Find out what those packets contain by following one of the conversations (also called network flows), select one of the packets and press the right mouse button..click on follow."
</blockquote>

<h2>6.1 Protocol Display Filters</h2>
<p>
Wireshark provides boolean filtering expressions to selectively display traffic streams:
</p>

<table style="width: 100%; border: none;">
    <tr>
        <td style="width: 50%; border: none; padding: 5px;">
            <div class="figure-container">
                <img src="{img_filt_http}" alt="HTTP Filter">
                <div class="caption">Figure 6a: HTTP display filter (<code>http</code>).</div>
            </div>
        </td>
        <td style="width: 50%; border: none; padding: 5px;">
            <div class="figure-container">
                <img src="{img_filt_tcp}" alt="TCP Filter">
                <div class="caption">Figure 6b: TCP display filter (<code>tcp</code>).</div>
            </div>
        </td>
    </tr>
    <tr>
        <td style="width: 50%; border: none; padding: 5px;">
            <div class="figure-container">
                <img src="{img_filt_dns}" alt="DNS Filter">
                <div class="caption">Figure 6c: DNS display filter (<code>dns</code>).</div>
            </div>
        </td>
        <td style="width: 50%; border: none; padding: 5px;">
            <div class="figure-container">
                <img src="{img_filt_udp}" alt="UDP Filter">
                <div class="caption">Figure 6d: UDP display filter (<code>udp</code>).</div>
            </div>
        </td>
    </tr>
</table>

<h2>6.2 Part 5a: Following UDP Stream (Conversation Reassembly)</h2>
<p>
Selecting a packet and navigating to <strong>Right Click &rarr; Follow &rarr; UDP Stream</strong> reassembles the fragmented payload bytes, strips lower protocol headers, and presents the unencapsulated application data in UTF-8.
</p>

<div class="figure-container">
    <img src="{img_follow}" alt="Follow UDP Stream 8">
    <div class="caption">Figure 7: Wireshark stream reconstruction dialog for UDP Stream 8 (<code>udp.stream eq 8</code>) capturing mDNS service discovery announcements.</div>
</div>

<p><strong>Analysis of Reassembled Stream 8 Payload:</strong></p>
<p>The stream captures Multicast DNS (mDNS) announcements broadcast over UDP port 5353 by an office printer:</p>
<ul>
    <li><strong>Device Name:</strong> <code>HP Smart Tank 580-590 series [5103FC]</code></li>
    <li><strong>Hostname:</strong> <code>HPBC0FF35103FC.local.</code></li>
    <li><strong>Hardware MAC:</strong> <code>bc:0f:f3:51:03:fc</code></li>
    <li><strong>Advertised Service Descriptors:</strong>
        <ul>
            <li><code>_uscans._tcp.local</code> (eSCL network scanning)</li>
            <li><code>_ipps._tcp.local</code> (Secure IPP printing)</li>
            <li><code>_ipp._tcp.local</code> (Standard IPP printing)</li>
        </ul>
    </li>
    <li><strong>Configuration Attributes:</strong> Admin URL: <code>http://HPBC0FF35103FC.local./#hId-pgAirPrint</code>, UUID: <code>6efecd0c-04a3-4ef1-9e30-33a369ee61b3</code>, Color=T, Duplex=F, Scan=T.</li>
    <li><strong>Supported PDLs:</strong> <code>application/octet-stream</code>, <code>application/pdf</code>, <code>image/jpeg</code>, <code>application/vnd.hp-PCL</code>, <code>image/urf</code>.</li>
</ul>

<div class="page-break"></div>

<!-- QUESTION 6 & 7 & 8 -->
<h1>7. Question 6: Server HTTP Response and Ethernet Layer Inspection</h1>
<blockquote>
"Search through your capture, and find an HTTP packet coming back from the server (TCP Source Port == 80). Expand the Ethernet layer in the Packet Details Panel."
</blockquote>
<p>
The display filter <code>http and tcp.srcport == 80</code> was applied. Packet No. 224 was selected and the <strong>Ethernet II</strong> layer was expanded.
</p>

<div class="figure-container">
    <img src="{img_q6_eth}" alt="Server HTTP Response Ethernet Layer">
    <div class="caption">Figure 8: Packet 224 matching 'http and tcp.srcport == 80' with expanded Ethernet II layer.</div>
</div>

<div class="figure-container">
    <img src="{img_q6_bits}" alt="Ethernet Header Bit Flags">
    <div class="caption">Figure 9: Detailed bit-level inspection of Destination and Source addresses in the Ethernet II header.</div>
</div>

<p><strong>Ethernet II Header Dissection:</strong></p>
<ul>
    <li><strong>Destination MAC Address (PC):</strong> <code>LiteonTechno_af:91:15 (d8:f3:bc:af:91:15)</code>
        <ul>
            <li><code>LG bit = 0</code>: Globally unique address (factory default IEEE assigned)</li>
            <li><code>IG bit = 0</code>: Individual address (unicast frame)</li>
        </ul>
    </li>
    <li><strong>Source MAC Address (Sending Node):</strong> <code>HewlettPacka_b7:a5:00 (80:30:e0:b7:a5:00)</code>
        <ul>
            <li><code>LG bit = 0</code>: Globally unique address (factory default IEEE assigned)</li>
            <li><code>IG bit = 0</code>: Individual address (unicast frame)</li>
        </ul>
    </li>
    <li><strong>EtherType:</strong> <code>IPv4 (0x0800)</code></li>
</ul>

<h1>8. Question 7: NIC Manufacturers of PC and Server</h1>
<blockquote>
"What are the manufacturers of your PC’s Network Interface Card (NIC), and the servers NIC?"
</blockquote>
<p>Referring to the expanded Ethernet II header in Figures 8 and 9:</p>
<ol>
    <li><strong>PC's Network Interface Card (NIC):</strong>
        <ul>
            <li><strong>MAC Address:</strong> <code>d8:f3:bc:af:91:15</code></li>
            <li><strong>Manufacturer:</strong> <strong>Lite-On Technology Corp.</strong> (<code>LiteonTechno</code>) &mdash; corresponds to the laptop's internal wireless adapter (<code>wlp1s0</code>).</li>
        </ul>
    </li>
    <li><strong>Server's / Gateway's Network Interface Card (NIC):</strong>
        <ul>
            <li><strong>MAC Address:</strong> <code>80:30:e0:b7:a5:00</code></li>
            <li><strong>Manufacturer:</strong> <strong>Hewlett Packard</strong> (<code>HewlettPacka</code>)</li>
        </ul>
    </li>
</ol>

<div class="highlight-box">
    <strong>Key Networking Principle (Hop-by-Hop MAC Rewriting):</strong><br>
    Layer-2 MAC addresses <strong>never cross IP router boundaries</strong>. The web server (<code>151.101.193.91</code>) is located on a remote Fastly CDN datacenter across the Internet. Intermediate routers rewrite the Layer-2 Ethernet frame at every hop. Therefore, the Source MAC (<code>80:30:e0:b7:a5:00</code>) belongs to the <strong>local default gateway router / wireless access point</strong> (manufactured by <strong>Hewlett Packard</strong>), rather than the physical NIC of the remote web server.
</div>

<div class="page-break"></div>

<!-- QUESTION 8 -->
<h1>9. Question 8: Organizationally Unique Identifiers (OUIs)</h1>
<blockquote>
"What are the Hex values (shown in the raw bytes panel) of the two NICS Manufacturers OUIs?"
</blockquote>
<p>
An <strong>Organizationally Unique Identifier (OUI)</strong> comprises the first <strong>24 bits (3 bytes)</strong> of a standard 48-bit IEEE 802 MAC address, assigned to hardware vendors by the IEEE Registration Authority.
</p>

<table style="width: 100%; border: none;">
    <tr>
        <td style="width: 50%; border: none; padding: 5px;">
            <div class="figure-container">
                <img src="{img_q8_pc}" alt="PC NIC OUI in Raw Bytes">
                <div class="caption">Figure 10a: PC NIC MAC 'd8 f3 bc af 91 15' highlighted in raw bytes.</div>
            </div>
        </td>
        <td style="width: 50%; border: none; padding: 5px;">
            <div class="figure-container">
                <img src="{img_q8_gw}" alt="Gateway NIC OUI in Raw Bytes">
                <div class="caption">Figure 10b: Gateway NIC MAC '80 30 e0 b7 a5 00' highlighted in raw bytes.</div>
            </div>
        </td>
    </tr>
</table>

<table class="data-table">
    <thead>
        <tr>
            <th>Node Role</th>
            <th>Full MAC Address</th>
            <th>OUI (Hex Value)</th>
            <th>OUI Bytes</th>
            <th>Manufacturer Identity</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td><strong>PC Client NIC</strong></td>
            <td><code>d8:f3:bc:af:91:15</code></td>
            <td><strong><code>d8:f3:bc</code></strong></td>
            <td><code>0xd8, 0xf3, 0xbc</code></td>
            <td>Lite-On Technology Corp.</td>
        </tr>
        <tr>
            <td><strong>Gateway / Server NIC</strong></td>
            <td><code>80:30:e0:b7:a5:00</code></td>
            <td><strong><code>80:30:e0</code></strong></td>
            <td><code>0x80, 0x30, 0xe0</code></td>
            <td>Hewlett Packard Enterprise</td>
        </tr>
    </tbody>
</table>

<div class="highlight-box">
    <strong>Answers:</strong><br>
    &bull; Hex value of PC NIC Manufacturer OUI: <strong><code>d8:f3:bc</code></strong> (or <code>d8 f3 bc</code>)<br>
    &bull; Hex value of Gateway / Server NIC Manufacturer OUI: <strong><code>80:30:e0</code></strong> (or <code>80 30 e0</code>)
</div>

<!-- QUESTION 9 -->
<h1>10. Question 9: Protocol Hierarchy Statistics</h1>
<blockquote>
"Find the following statistics:<br>
a. What percentage of packets in your capture are TCP, and give an example of the higher level protocol which uses TCP?<br>
b. What percentage of packets in your capture are UDP, and give an example of the higher level protocol which uses UDP?"
</blockquote>

<p>
Navigating to <strong>Statistics &rarr; Protocol Hierarchy</strong> generates an aggregated breakdown of all 94,876 captured packets (totaling 59,833,276 bytes).
</p>

<table style="width: 100%; border: none;">
    <tr>
        <td style="width: 50%; border: none; padding: 5px;">
            <div class="figure-container">
                <img src="{img_q9_tcp}" alt="TCP Hierarchy Statistics">
                <div class="caption">Figure 11a: Protocol Hierarchy highlighting TCP (26.9% packets, 67.4% bytes).</div>
            </div>
        </td>
        <td style="width: 50%; border: none; padding: 5px;">
            <div class="figure-container">
                <img src="{img_q9_udp}" alt="UDP Hierarchy Statistics">
                <div class="caption">Figure 11b: Protocol Hierarchy highlighting UDP (72.3% packets, 0.9% bytes).</div>
            </div>
        </td>
    </tr>
</table>

<div class="page-break"></div>

<p><strong>Statistical Summary:</strong></p>
<ul>
    <li><strong>Part a: Transmission Control Protocol (TCP):</strong>
        <ul>
            <li><strong>Packet Percentage:</strong> <strong><code>26.9%</code></strong> (25,508 packets out of 94,876)</li>
            <li><strong>Byte Volume:</strong> 40,300,500 bytes (<strong>67.4%</strong> of all captured bytes)</li>
            <li><strong>Higher-Level Protocols Using TCP:</strong>
                <ul>
                    <li><strong>TLS (Transport Layer Security):</strong> 4,362 packets (4.6%)</li>
                    <li><strong>HTTP (Hypertext Transfer Protocol):</strong> 505 packets (0.5%)</li>
                    <li>Subordinate web payloads: PNG images, XML files, CSS stylesheets</li>
                </ul>
            </li>
        </ul>
    </li>
    <li><strong>Part b: User Datagram Protocol (UDP):</strong>
        <ul>
            <li><strong>Packet Percentage:</strong> <strong><code>72.3%</code></strong> (68,559 packets out of 94,876)</li>
            <li><strong>Byte Volume:</strong> 548,472 bytes (<strong>0.9%</strong> of all captured bytes)</li>
            <li><strong>Higher-Level Protocols Using UDP:</strong>
                <ul>
                    <li><strong>mDNS (Multicast DNS):</strong> 47,767 packets (50.3%)</li>
                    <li><strong>SSDP (Simple Service Discovery):</strong> 12,653 packets (13.3%)</li>
                    <li><strong>NetBIOS Name Service (NBNS):</strong> 1,765 packets (1.9%)</li>
                    <li><strong>LLMNR (Link-Local Multicast Name Resolution):</strong> 1,598 packets (1.7%)</li>
                    <li><strong>DNS (Domain Name System):</strong> 590 packets (0.6%)</li>
                    <li><strong>QUIC IETF:</strong> 196 packets (0.2%)</li>
                    <li><strong>NTP (Network Time Protocol):</strong> 20 packets (&lt;0.1%)</li>
                </ul>
            </li>
        </ul>
    </li>
</ul>

<table class="data-table">
    <thead>
        <tr>
            <th>Protocol</th>
            <th>Packets</th>
            <th>% of Packets</th>
            <th>Bytes</th>
            <th>% of Bytes</th>
            <th>Avg Size</th>
            <th>Primary Role in Trace</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td><strong>UDP</strong></td>
            <td>68,559</td>
            <td><strong>72.3%</strong></td>
            <td>548,472</td>
            <td><strong>0.9%</strong></td>
            <td>&asymp; 8 B</td>
            <td>High-frequency lightweight multicast discovery and beaconing.</td>
        </tr>
        <tr>
            <td><strong>TCP</strong></td>
            <td>25,508</td>
            <td><strong>26.9%</strong></td>
            <td>40,300,500</td>
            <td><strong>67.4%</strong></td>
            <td>&asymp; 1580 B</td>
            <td>Heavyweight reliable web browsing, TLS, and media transfers.</td>
        </tr>
        <tr>
            <td><strong>Other / LLC</strong></td>
            <td>809</td>
            <td>0.8%</td>
            <td>18,984,304</td>
            <td>31.7%</td>
            <td>&mdash;</td>
            <td>Link-layer control, IPv6, HP switch protocols.</td>
        </tr>
    </tbody>
</table>

<!-- QUESTION 10 -->
<h1>11. Question 10: Traffic Flow Graph Analysis</h1>
<blockquote>
"Find the traffic flow Select the Statistics->Flow Graph menu option. Choose General Flow and Network Source options, and click the OK button."
</blockquote>
<p>
The <strong>Flow Graph</strong> renders a two-dimensional ladder diagram plotting temporal packet exchanges between network endpoints along vertical timeline axes.
</p>

<h2>11.1 General Flow Graph (All Flows, Network Addresses)</h2>
<div class="figure-container">
    <img src="{img_q10_gen}" alt="General Flow Graph">
    <div class="caption">Figure 12: Wireshark General Flow Graph (All Flows, Network Addresses) showing multicast discovery traffic.</div>
</div>
<p>
Multiple local IP endpoints (<code>172.28.23.186</code>, <code>172.28.17.108</code>, <code>172.28.28.60</code>, <code>172.28.23.71</code>, <code>172.28.23.113</code>) interact with the mDNS multicast IP <code>224.0.0.251</code>, showing active zero-configuration host and service discovery across the campus subnet.
</p>

<div class="page-break"></div>

<h2>11.2 TCP Flow Graph (TCP Connection Lifecycle)</h2>
<div class="figure-container">
    <img src="{img_q10_tcp}" alt="TCP Flow Graph">
    <div class="caption">Figure 13: Wireshark TCP Flow Graph showing complete connection lifecycle (Handshake &rarr; Data Transfer &rarr; Teardown).</div>
</div>

<p><strong>Detailed Lifecycle Phases:</strong></p>
<ol>
    <li><strong>Phase 1: Three-Way Handshake (Establishment):</strong>
        <ul>
            <li>Client &rarr; Server: <code>SYN</code> (port <code>39204 &rarr; 80</code>, <code>Seq=0</code>)</li>
            <li>Server &rarr; Client: <code>SYN, ACK</code> (port <code>80 &rarr; 39204</code>, <code>Seq=0, Ack=1</code>)</li>
            <li>Client &rarr; Server: <code>ACK</code> (port <code>39204 &rarr; 80</code>, <code>Seq=1, Ack=1</code>)</li>
        </ul>
    </li>
    <li><strong>Phase 2: HTTP Transaction (Data Exchange):</strong>
        <ul>
            <li>Client &rarr; Server: <code>PSH, ACK - Len: 295</code> (HTTP GET request)</li>
            <li>Server &rarr; Client: <code>ACK</code> (acknowledging request receipt)</li>
            <li>Server &rarr; Client: <code>PSH, ACK - Len: 322</code> (HTTP 204 OK response)</li>
        </ul>
    </li>
    <li><strong>Phase 3: Connection Teardown (Four-Way Termination):</strong>
        <ul>
            <li>Server &rarr; Client: <code>FIN, ACK</code> (Server initiates close, <code>Seq=323, Ack=296</code>)</li>
            <li>Client &rarr; Server: <code>ACK</code> (Client acknowledges close, <code>Seq=296, Ack=323</code>)</li>
            <li>Client &rarr; Server: <code>FIN, ACK</code> (Client initiates close, <code>Seq=296, Ack=324</code>)</li>
            <li>Server &rarr; Client: <code>ACK</code> (Server completes termination, <code>Seq=324, Ack=297</code>)</li>
        </ul>
    </li>
</ol>

<!-- SECTION 12 -->
<h1>12. Conclusion and Key Learnings</h1>
<ol>
    <li><strong>Layer Encapsulation:</strong> Confirmed how application data (HTTP/DNS) is encapsulated inside transport headers (TCP/UDP), which are nested inside network packets (IPv4), and wrapped in data link frames (Ethernet II).</li>
    <li><strong>ARP and Link-Layer Dynamics:</strong> Address resolution via ARP broadcasts is indispensable for mapping logical IP addresses to 48-bit physical MAC addresses on shared broadcast domains.</li>
    <li><strong>Hop-by-Hop MAC Rewriting:</strong> Wireshark inspection reinforced the fundamental principle that MAC addresses do not survive IP routing boundaries; the source MAC of an inbound Internet packet identifies the local default router rather than the remote server.</li>
    <li><strong>Transport Trade-offs:</strong> Evidenced a clear division between TCP (used for reliable, bulky HTTP/TLS data transfer accounting for 67.4% of total bytes) and UDP (used for lightweight, high-frequency multicast device discovery and name resolution accounting for 72.3% of total packets).</li>
    <li><strong>Conversational Reassembly:</strong> Following TCP/UDP streams removes protocol encapsulation overhead, enabling complete inspection of application-layer payloads and REST transactions.</li>
</ol>

</body>
</html>
"""

with open("report_input.html", "w", encoding="utf-8") as f:
    f.write(html_content)

print("report_input.html written successfully!")
cmd = [
    "libreoffice",
    "-env:UserInstallation=file:///tmp/libo_user",
    "--headless",
    "--convert-to",
    "docx:Office Open XML Text",
    "report_input.html"
]
env = os.environ.copy()
env["XDG_RUNTIME_DIR"] = "/tmp"

res = subprocess.run(cmd, env=env, capture_output=True, text=True)
print("STDOUT:", res.stdout)
print("STDERR:", res.stderr)
if os.path.exists("report_input.docx"):
    os.rename("report_input.docx", "Assignment5_Report.docx")
    print("Assignment5_Report.docx created successfully!")
