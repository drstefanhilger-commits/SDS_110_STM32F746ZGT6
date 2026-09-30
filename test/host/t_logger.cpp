/*
 * t_logger – Logger-Ringpuffer
 *
 * Bezug: Befund 19
 * Prüft/misst: Längenbegrenzung 255 ohne Nullbyte; 4 Schreib-Threads + 1 Leser: keine defekten/vertauschten Meldungen, empfangen + verworfen = gesendet
 * Aufruf: make check  bzw.  build/test_host/t_logger
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include "Infrastructure/Utils/Logger.hpp"
#include <type_traits>
#include <utility>
// dropped() gibt es erst seit Befund 19; für Gegenproben gegen ältere Stände optional aufrufen
template <typename L, typename = void> struct HasDropped : std::false_type {};
template <typename L> struct HasDropped<L, std::void_t<decltype(std::declval<L&>().dropped())>> : std::true_type {};
template <typename L> static long droppedOf(L& l) { if constexpr (HasDropped<L>::value) return (long)l.dropped(); else return 0; }
int main()
{
    Logger& log = Logger::instance(); int fail = 0;
    // 1) Längenbegrenzung: 300 Zeichen -> 255, kein Nullbyte im Puffer
    std::string big(300, 'x');
    log.write("%s", big.c_str());
    uint8_t buf[512]; int n = log.read(buf, sizeof(buf));
    bool ok = n == 255 && std::memchr(buf, 0, n) == nullptr;
    std::printf("300 Zeichen -> %d Bytes, kein Nullbyte: %s\n", n, ok ? "OK" : "FEHLER"); fail += !ok;

    // 2) 4 Schreiber + 1 Leser parallel
    constexpr int W = 4, M = 20000;
    std::atomic<bool> done{false}; std::string rx; std::atomic<long> bytes{0};
    std::thread reader([&] { uint8_t b[128]; for (;;) { int k = log.read(b, sizeof(b)); if (k > 0) rx.append((char*)b, k); else if (done) { if ((k = log.read(b, sizeof(b))) <= 0) break; rx.append((char*)b, k); } } });
    std::vector<std::thread> ws;
    for (int w = 0; w < W; ++w) ws.emplace_back([&, w] { for (int i = 0; i < M; ++i) log.write("<W%d#%06d:%s>", w, i, "abcdefghijklmnopqrstuvwxyz0123456789"); });
    for (auto& t : ws) t.join();
    done = true; reader.join();
    // Meldungen prüfen: jede muss vollständig und unvermischt sein
    long msgs = 0, bad = 0; size_t p = 0; std::vector<int> last(W, -1); long order = 0;
    while (p < rx.size()) {
        size_t e = rx.find('>', p);
        if (rx[p] != '<' || e == std::string::npos) { ++bad; break; }
        int w, i; char tail[64];
        if (std::sscanf(rx.c_str() + p, "<W%d#%d:%36[^>]>", &w, &i, tail) != 3 || std::strcmp(tail, "abcdefghijklmnopqrstuvwxyz0123456789") != 0) ++bad;
        else { if (i <= last[w]) ++order; last[w] = i; }
        ++msgs; p = e + 1;
    }
    const long sent = (long)W * M, dropped = droppedOf(log);
    ok = bad == 0 && order == 0 && msgs + dropped == sent;
    std::printf("gesendet %ld, empfangen %ld, verworfen %ld, defekt %ld, Reihenfolge verletzt %ld: %s\n", sent, msgs, dropped, bad, order, ok ? "OK" : "FEHLER");
    fail += !ok;
    return fail;
}
