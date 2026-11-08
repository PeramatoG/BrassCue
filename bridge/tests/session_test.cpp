// BrassCue - GPL-3.0. Deterministic integration across control, BC1 and output.
#include "../src/controller_session.hpp"
#include "../src/output_session.hpp"
#include "../src/serial_session.hpp"
#include "check.hpp"
#include <vector>
int main(){
 using namespace brasscue;
 ControllerSession session;
 session.encoder(0,255);session.fader(128);
 CHECK(session.engine().render(0)[1][0]==128);
 auto p=session.profile();p.fader.assignments={{{2,512},200}};session.configure(p);
 CHECK(!session.engine().render(0).count(2)); // GM ignores its patch completely.
 p.fader.grandmaster=false;session.configure(p);
 CHECK(session.engine().render(0)[1][0]==255 && session.engine().render(0)[2][511]==100);
 session.button(0,true,0);session.button(0,true,0);
 CHECK(session.engine().render(0)[1][4]==255);
 session.button(0,false,0);session.button(1,true,0);
 CHECK(session.engine().render(0)[1][4]==128);
 session.button(1,false,0);session.button(2,true,0);
 CHECK(session.engine().render(0)[1][4]==0 && session.engine().render(0)[1][0]==255);
 session.button(2,false,0);session.button(2,true,0);
 CHECK(std::string(session.engine().button_status(2,0))=="OFF");
 p.buttons[0].fade_in_ms=1000;p.buttons[0].fade_out_ms=1000;
 p.buttons[1].fade_in_ms=1000;p.buttons[1].fade_out_ms=1000;
 session.configure(p);session.button(0,true,100);
 CHECK(session.engine().render(100)[1][4]==0);
 CHECK(session.engine().render(600)[1][4]==128);
 session.button(0,false,600);session.button(0,true,600); // reverse from midpoint
 CHECK(session.engine().render(600)[1][4]==128);
 CHECK(session.engine().render(1100)[1][4]==64);
 session.configure(p);session.button(0,true,0);session.button(0,false,1000);session.button(1,true,1000);
 CHECK(session.engine().render(1500)[1][4]==128); // outgoing HTP envelope
 CHECK(session.engine().render(2000)[1][4]==128);
 session.button(1,false,2000);session.button(2,true,2000);
 CHECK(session.engine().render(2500)[1][4]==64 && session.engine().render(3000)[1][4]==0);
 p.buttons[1].kill=false;p.buttons[0].fade_in_ms=0;p.buttons[0].fade_out_ms=0;
 session.configure(p);session.button(0,true,0);session.button(1,true,0);
 CHECK(session.engine().render(1000)[1][4]==255); // HTP, coexistence
 p.buttons[0].mode=ButtonMode::Flash;session.configure(p);
 CHECK(session.event({EventType::Button,0,1},false,0));
 CHECK(session.engine().render(0)[1][4]==255);
 session.release_all(10);CHECK(session.engine().render(10)[1][4]==0);
 CHECK(!session.event({EventType::Grandmaster,0,0},true,0));
 p.encoders[0]={{{32768,512},128}};session.configure(p);session.encoder(0,128);
 auto bytes=session.engine().packet(32768,0);
 CHECK(bytes[14]==255 && bytes[15]==127 && bytes[529]==64);
 OutputSession output;std::vector<std::array<std::uint8_t,kPacketBytes>> sent;
 auto send=[&](const auto& packet){sent.push_back(packet);};
 output.update(session.engine().render(0),send);sent.clear();
 p.encoders[0]={{{2,1},255}};session.configure(p);output.update(session.engine().render(0),send);
 bool cleared=false;for(const auto& packet:sent)if(packet[14]==255&&packet[15]==127){cleared=true;for(std::size_t i=18;i<packet.size();++i)CHECK(packet[i]==0);}
 CHECK(cleared);CHECK(!output.enabled());
 for(const auto& ip:{"invalid","256.0.0.1"}){bool failed=false;try{UdpTarget target(ip);}catch(const std::exception&){failed=true;}CHECK(failed);}
 BoundedSerialLine line;std::string record;
 for(char c:std::string(81,'x'))CHECK(!line.feed(c,record));
 CHECK(!line.feed('\n',record));
 for(char c:std::string("BC1 READY"))CHECK(!line.feed(c,record));
 CHECK(line.feed('\n',record)&&record=="BC1 READY");
 SerialHandshake handshake(100);
 CHECK(!handshake.hello_due(2599));CHECK(handshake.hello_due(2600));CHECK(!handshake.hello_due(2700));
 CHECK(handshake.ready(2800));CHECK(!handshake.ready(2801));CHECK(handshake.ready(4000));CHECK(handshake.connected());
}
