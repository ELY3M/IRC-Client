/*
Copyright (C) 2026-Forever ELY M. (ELY3M at github)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://gnu.org>.
*/

#include <afxwin.h>
#include <afxcmn.h>
#include <afxsock.h>
#include <afxext.h>
#include <afxdlgs.h>
#include <map>
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#define SECURITY_WIN32
#include <sspi.h>
#include <schannel.h>
#include <shellapi.h>
#include <shlobj.h>
#include <ws2tcpip.h>
#include <windns.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "shell32.lib")
#include <gdiplus.h>
#include <mmsystem.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")     // MCI (sound playback) -- see /splay, /vol, $vol, $inwave/$inmidi/$insong
#pragma comment(lib, "ole32.lib")     // Core Audio (master volume/mute) -- see /vol -v
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")           // prevents a console window regardless of the /link command used
#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")   // Unicode MFC entry point (VS sets this automatically)


#define VERSION L"IRC Client - https://github.com/ELY3M/IRC-Client"
#define DEFAULT_FONT L"Fixedsys"

// These are plain (non-const) globals rather than compile-time constants so the Colors dialog can change them at
// runtime; every existing call site that uses one as a default parameter value still works unchanged, since C++
// re-reads a default argument's current value at each call rather than requiring it to be a compile-time constant.
static COLORREF cText = RGB(0, 0, 0), 
                cCTCP = RGB(255, 0, 0),
                cHighlight = RGB(127, 0, 0),
                cInvite = RGB(0, 147, 0),
                cJoin = RGB(0, 147,0),
                cPart = RGB(0, 147,0),
                cQuit = RGB(0, 0, 127),
                cMode = RGB(0, 147, 0),
                cTopic = RGB(0, 147, 0),
                cKick = RGB(0, 147, 0),
                cNickname = RGB(0, 147, 0),
                cOwn = RGB(0, 0, 0),
                cNotice = RGB(127, 0, 0), 
                cAction = RGB(156, 0, 156),
                cOther = RGB(156, 0, 156),
                cInfo = RGB(0, 0, 127),
                cInfo2 = RGB(0, 147, 0),
                cWallops = RGB(127, 0, 0),
                cWhois = RGB(0, 0, 0);

// Remove mIRC control codes (bold, color, reverse, italic, strikethrough, underline, reset)
static CString Strip(const CString& s) {
    CString o; int n = s.GetLength();
    auto dig = [&](int j) { return j < n && iswdigit(s[j]); };
    for (int i = 0; i < n; i++) {
        wchar_t c = s[i];
        if (c == 3) {
            int k = 0; while (k < 2 && dig(i + 1)) { i++; k++; }
            if (k && i + 1 < n && s[i + 1] == L',' && dig(i + 2)) { i += 2; if (dig(i + 1)) i++; }
        } else if (c == 2 || c == 15 || c == 22 || c == 29 || c == 30 || c == 31) {}
        else o += c;
    }
    return o;
}
// Draws mIRC color-coded text into a plain GDI device context (used by the /list window's Topic
// column, since a CListCtrl can't render per-character colors on its own the way the chat log can).
// Only foreground color changes are rendered; bold/underline/italic/reverse are ignored here for simplicity.
// Many ircds (UnrealIRCd and others, including Rizon) embed a channel's modes directly in the topic
// field of the LIST reply, as a leading bracketed prefix like "[+ntr]" or "[+lk 50]". This pulls that
// out so it can go in its own column, the same way mIRC's channel list window does -- no extra
// per-channel MODE query needed (which wouldn't scale to a list of thousands of channels anyway).
static bool ParseListModes(const CString& topicIn, CString& modesOut, CString& topicOut) {
    if (topicIn.Left(1) == L"[") {
        int end = topicIn.Find(L']');
        if (end > 0) {
            CString inside = topicIn.Mid(1, end - 1);
            if (!inside.IsEmpty() && inside[0] == L'+') {
                modesOut = inside;
                topicOut = topicIn.Mid(end + 1); topicOut.TrimLeft();
                return true;
            }
        }
    }
    modesOut.Empty(); topicOut = topicIn;
    return false;
}
// $os: Windows version in mIRC's naming (XP, 2003, 2003R2, Vista, 2008, 7, 2008R2, 8, 2012, 8.1, 2012R2, 10, 11, 2016, 2019, 2022).
// Uses RtlGetVersion because GetVersionEx reports a fake version unless the exe carries a compatibility manifest.
static CString OsName() {
    typedef LONG (WINAPI* RtlGetVersionFn)(PRTL_OSVERSIONINFOW);
    OSVERSIONINFOEXW vi = {}; vi.dwOSVersionInfoSize = sizeof vi;
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn fn = nt ? (RtlGetVersionFn)GetProcAddress(nt, "RtlGetVersion") : nullptr;
    if (!fn || fn((PRTL_OSVERSIONINFOW)&vi) != 0) return CString();
    bool wk = vi.wProductType == VER_NT_WORKSTATION;
    DWORD maj = vi.dwMajorVersion, mnr = vi.dwMinorVersion, bld = vi.dwBuildNumber;
    if (maj == 5 && mnr == 1) return CString(L"XP");
    if (maj == 5 && mnr == 2) return CString(wk ? L"XP" : (GetSystemMetrics(SM_SERVERR2) ? L"2003R2" : L"2003"));
    if (maj == 6 && mnr == 0) return CString(wk ? L"Vista" : L"2008");
    if (maj == 6 && mnr == 1) return CString(wk ? L"7" : L"2008R2");
    if (maj == 6 && mnr == 2) return CString(wk ? L"8" : L"2012");
    if (maj == 6 && mnr == 3) return CString(wk ? L"8.1" : L"2012R2");
    if (maj == 10) {
        if (wk) return CString(bld >= 22000 ? L"11" : L"10");
        return CString(bld >= 26100 ? L"2025" : bld >= 20348 ? L"2022" : bld >= 17763 ? L"2019" : L"2016");
    }
    return CString();
}
// mIRC's full 99-colour palette: 0-15 are the classic set (same values used elsewhere in this file for ^C rendering);
// 16-98 are the fixed extended colours, standardized across clients (values per the modern IRC formatting spec).
static const COLORREF kMircPalette[99] = {
    RGB(255,255,255), RGB(0,0,0), RGB(0,0,127), RGB(0,147,0), RGB(255,0,0), RGB(127,0,0), RGB(156,0,156), RGB(252,127,0),
    RGB(255,255,0), RGB(0,252,0), RGB(0,147,147), RGB(0,255,255), RGB(0,0,252), RGB(255,0,255), RGB(127,127,127), RGB(210,210,210),
    RGB(0x47,0x00,0x00), RGB(0x47,0x21,0x00), RGB(0x47,0x47,0x00), RGB(0x32,0x47,0x00), RGB(0x00,0x47,0x00), RGB(0x00,0x47,0x2c),
    RGB(0x00,0x47,0x47), RGB(0x00,0x27,0x47), RGB(0x00,0x00,0x47), RGB(0x2e,0x00,0x47), RGB(0x47,0x00,0x47), RGB(0x47,0x00,0x2a),
    RGB(0x74,0x00,0x00), RGB(0x74,0x3a,0x00), RGB(0x74,0x74,0x00), RGB(0x51,0x74,0x00), RGB(0x00,0x74,0x00), RGB(0x00,0x74,0x49),
    RGB(0x00,0x74,0x74), RGB(0x00,0x40,0x74), RGB(0x00,0x00,0x74), RGB(0x4b,0x00,0x74), RGB(0x74,0x00,0x74), RGB(0x74,0x00,0x45),
    RGB(0xb5,0x00,0x00), RGB(0xb5,0x63,0x00), RGB(0xb5,0xb5,0x00), RGB(0x7d,0xb5,0x00), RGB(0x00,0xb5,0x00), RGB(0x00,0xb5,0x71),
    RGB(0x00,0xb5,0xb5), RGB(0x00,0x63,0xb5), RGB(0x00,0x00,0xb5), RGB(0x75,0x00,0xb5), RGB(0xb5,0x00,0xb5), RGB(0xb5,0x00,0x6b),
    RGB(0xff,0x00,0x00), RGB(0xff,0x8c,0x00), RGB(0xff,0xff,0x00), RGB(0xb2,0xff,0x00), RGB(0x00,0xff,0x00), RGB(0x00,0xff,0xa0),
    RGB(0x00,0xff,0xff), RGB(0x00,0x8c,0xff), RGB(0x00,0x00,0xff), RGB(0xa5,0x00,0xff), RGB(0xff,0x00,0xff), RGB(0xff,0x00,0x98),
    RGB(0xff,0x59,0x59), RGB(0xff,0xb4,0x59), RGB(0xff,0xff,0x71), RGB(0xcf,0xff,0x60), RGB(0x6f,0xff,0x6f), RGB(0x65,0xff,0xc9),
    RGB(0x6d,0xff,0xff), RGB(0x59,0xb4,0xff), RGB(0x59,0x59,0xff), RGB(0xc4,0x59,0xff), RGB(0xff,0x66,0xff), RGB(0xff,0x59,0xbc),
    RGB(0xff,0x9c,0x9c), RGB(0xff,0xd3,0x9c), RGB(0xff,0xff,0x9c), RGB(0xe2,0xff,0x9c), RGB(0x9c,0xff,0x9c), RGB(0x9c,0xff,0xdb),
    RGB(0x9c,0xff,0xff), RGB(0x9c,0xd3,0xff), RGB(0x9c,0x9c,0xff), RGB(0xdc,0x9c,0xff), RGB(0xff,0x9c,0xff), RGB(0xff,0x94,0xd3),
    RGB(0x00,0x00,0x00), RGB(0x13,0x13,0x13), RGB(0x28,0x28,0x28), RGB(0x36,0x36,0x36), RGB(0x4d,0x4d,0x4d), RGB(0x65,0x65,0x65),
    RGB(0x81,0x81,0x81), RGB(0x9f,0x9f,0x9f), RGB(0xbc,0xbc,0xbc), RGB(0xe2,0xe2,0xe2), RGB(0xff,0xff,0xff)
};
// A ^C color number can be anything 0-99 in the wild (mIRC accepts the full range); 0-98 index the real palette
// directly, and anything else (99 = "default", or an out-of-range typo) falls back to wrapping into the classic 16,
// which is at least never garbage.
static COLORREF MircColor(int n) { return kMircPalette[n >= 0 && n <= 98 ? n : (((n % 16) + 16) % 16)]; }
// mIRC-style timestamp tokens: HH (24h), h (12h, no leading zero), nn (minutes), ss (seconds), tt (am/pm).
static CString FormatTimestamp(const CString& fmt) {
    SYSTEMTIME st; ::GetLocalTime(&st);
    CString out = fmt;
    auto pad2 = [](int v) { CString s; s.Format(L"%02d", v); return s; };
    out.Replace(L"HH", pad2(st.wHour));
    out.Replace(L"nn", pad2(st.wMinute));
    out.Replace(L"ss", pad2(st.wSecond));
    out.Replace(L"tt", st.wHour < 12 ? L"am" : L"pm");
    int h12 = st.wHour % 12; if (h12 == 0) h12 = 12;
    CString h12s; h12s.Format(L"%d", h12); out.Replace(L"h", h12s);
    return out;
}

static void DrawMircText(CDC* dc, CRect r, const CString& s, COLORREF base) {
    COLORREF fg = base, bg = CLR_NONE; int x = r.left; CString seg; int n = s.GetLength();
    int saved = dc->SaveDC();
    dc->IntersectClipRect(r);   // keeps per-segment background fills from bleeding past this cell
    // Bold/italic/underline/strikethrough each need their own HFONT (a single font's style can't be changed per
    // character), derived from whatever font the cell is already using and built lazily as each combination is
    // actually hit, then cleaned up before returning. Reverse just swaps fg/bg, same as the chat log.
    HFONT baseFont = (HFONT)::GetCurrentObject(dc->GetSafeHdc(), OBJ_FONT);
    LOGFONT lf = {}; ::GetObjectW(baseFont, sizeof(lf), &lf);
    HFONT variants[16] = {};
    bool bold = false, italic = false, underline = false, strike = false;
    auto selectStyle = [&]() {
        int idx = (bold ? 1 : 0) | (italic ? 2 : 0) | (underline ? 4 : 0) | (strike ? 8 : 0);
        if (idx == 0) { ::SelectObject(dc->GetSafeHdc(), baseFont); return; }
        if (!variants[idx]) {
            LOGFONT v = lf;
            v.lfWeight = bold ? FW_BOLD : FW_NORMAL; v.lfItalic = italic; v.lfUnderline = underline; v.lfStrikeOut = strike;
            variants[idx] = ::CreateFontIndirectW(&v);
        }
        ::SelectObject(dc->GetSafeHdc(), variants[idx]);
    };
    auto flush = [&]() {
        if (seg.IsEmpty()) return;
        selectStyle();
        SIZE sz = dc->GetTextExtent(seg);
        CRect tr(x, r.top, x + sz.cx, r.bottom);
        if (bg != CLR_NONE) dc->FillSolidRect(tr, bg);
        dc->SetBkMode(TRANSPARENT);
        dc->SetTextColor(fg);
        dc->DrawText(seg, tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        x += sz.cx;
        seg.Empty();
    };
    auto num = [&](int& i) { int v = -1; for (int k = 0; k < 2 && i + 1 < n && iswdigit(s[i + 1]); k++) v = (v < 0 ? 0 : v * 10) + s[++i] - L'0'; return v; };
    for (int i = 0; i < n; i++) {
        wchar_t c = s[i];
        if (c == 3) {
            flush(); int f = num(i);
            if (f < 0) { fg = base; bg = CLR_NONE; continue; }
            fg = MircColor(f);
            if (i + 2 < n && s[i + 1] == L',' && iswdigit(s[i + 2])) { i++; bg = MircColor(num(i)); }   // background spec, now rendered
        }
        else if (c == 15) { flush(); fg = base; bg = CLR_NONE; bold = italic = underline = strike = false; }   // reset
        else if (c == 2) { flush(); bold = !bold; }
        else if (c == 29) { flush(); italic = !italic; }
        else if (c == 30) { flush(); strike = !strike; }
        else if (c == 31) { flush(); underline = !underline; }
        else if (c == 22) { flush(); COLORREF t = fg; fg = bg == CLR_NONE ? RGB(255,255,255) : bg; bg = t; }   // reverse
        else seg += c;
    }
    flush();
    ::SelectObject(dc->GetSafeHdc(), baseFont);
    for (HFONT v : variants) if (v) ::DeleteObject(v);
    dc->RestoreDC(saved);
}
static CString Word(CString& s) {
    s.TrimLeft(); int i = s.Find(L' '); CString w;
    if (i < 0) { w = s; s.Empty(); } else { w = s.Left(i); s = s.Mid(i + 1); }
    return w;
}
static CString Bare(CString s) { s.TrimLeft(L"@+%&~"); return s; }
// Splits off the first word for /run, honoring a "quoted path" the way mIRC does; what's left (untouched) becomes the parameters.
static CString RunWord(CString& s) {
    s.TrimLeft(); CString w;
    if (!s.IsEmpty() && s[0] == L'"') { int e = s.Find(L'"', 1); if (e > 0) { w = s.Mid(1, e - 1); s = s.Mid(e + 1); s.TrimLeft(); return w; } }
    int i = s.Find(L' ');
    if (i < 0) { w = s; s.Empty(); } else { w = s.Left(i); s = s.Mid(i + 1); }
    return w;
}
static CString IniPath(LPCWSTR name) {   // e.g. IniPath(L"servers.ini") -> full path next to the .exe
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    CString p = exe; return p.Left(p.ReverseFind(L'\\') + 1) + name;
}
// Loads any GDI+-supported image file (bmp/jpg/png/gif/...), scales it to fit w x h while preserving
// aspect ratio, and returns a plain GDI HBITMAP the caller owns. Returns nullptr if the file can't be read.
static HBITMAP LoadImageFileScaled(const CString& path, int w, int h) {
    Gdiplus::Bitmap src(path);
    if (src.GetLastStatus() != Gdiplus::Ok || src.GetWidth() == 0 || src.GetHeight() == 0) return nullptr;
    Gdiplus::Bitmap canvas(w, h, PixelFormat24bppRGB);
    Gdiplus::Graphics g(&canvas);
    g.Clear(Gdiplus::Color(255, 235, 240, 245));
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    double s = (std::min)((double)w / src.GetWidth(), (double)h / src.GetHeight());
    int dw = (int)(src.GetWidth() * s), dh = (int)(src.GetHeight() * s);
    g.DrawImage(&src, (w - dw) / 2, (h - dh) / 2, dw, dh);
    HBITMAP hb = nullptr;
    canvas.GetHBITMAP(Gdiplus::Color(255, 255, 255), &hb);
    return hb;
}

// ---------------- TLS client layer (Windows SChannel, no external libs) ----------------
class CTls {
public:
    CredHandle cred = {}; CtxtHandle ctx = {}; SecPkgContext_StreamSizes sz = {};
    bool hasCred = false, hasCtx = false, ready = false, lax = false; long lastStatus = 0;
    std::string in, out, tosend; CString host;
    ~CTls() { if (hasCtx) DeleteSecurityContext(&ctx); if (hasCred) FreeCredentialsHandle(&cred); }
    bool Init(const CString& h, bool l) {
        host = h; lax = l;
        SCHANNEL_CRED sc = {}; sc.dwVersion = SCHANNEL_CRED_VERSION;
        sc.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | (l ? SCH_CRED_MANUAL_CRED_VALIDATION : SCH_CRED_AUTO_CRED_VALIDATION);
        hasCred = AcquireCredentialsHandleW(nullptr, (LPWSTR)UNISP_NAME_W, SECPKG_CRED_OUTBOUND, nullptr, &sc, nullptr, nullptr, &cred, nullptr) == SEC_E_OK;
        return hasCred;
    }
    bool Handshake() {   // call once to get ClientHello, then again as server data arrives in 'in'
        for (;;) {
            SecBuffer ib[2] = { { (ULONG)in.size(), SECBUFFER_TOKEN, in.data() }, { 0, SECBUFFER_EMPTY, nullptr } };
            SecBufferDesc ibd = { SECBUFFER_VERSION, 2, ib };
            SecBuffer ob = { 0, SECBUFFER_TOKEN, nullptr };
            SecBufferDesc obd = { SECBUFFER_VERSION, 1, &ob };
            DWORD fl = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY |
                       ISC_REQ_STREAM | (lax ? ISC_REQ_MANUAL_CRED_VALIDATION : 0), at = 0;
            SECURITY_STATUS r = InitializeSecurityContextW(&cred, hasCtx ? &ctx : nullptr, (SEC_WCHAR*)(LPCWSTR)host, fl, 0, 0,
                                                           hasCtx ? &ibd : nullptr, 0, &ctx, &obd, &at, nullptr);
            if (r == SEC_E_INCOMPLETE_MESSAGE) return true;
            if (r != SEC_E_OK && r != SEC_I_CONTINUE_NEEDED && r != SEC_I_INCOMPLETE_CREDENTIALS) { lastStatus = r; return false; }
            hasCtx = true;
            if (ob.pvBuffer) { tosend.append((char*)ob.pvBuffer, ob.cbBuffer); FreeContextBuffer(ob.pvBuffer); }
            if (ib[1].BufferType == SECBUFFER_EXTRA) in.erase(0, in.size() - ib[1].cbBuffer); else in.clear();
            if (r == SEC_E_OK) { QueryContextAttributes(&ctx, SECPKG_ATTR_STREAM_SIZES, &sz); ready = true; return true; }
            // The server asked for a client certificate; we have none, so loop straight back and send that
            // "no certificate" reply immediately, even though there's no new server data waiting in 'in'.
            if (r == SEC_I_INCOMPLETE_CREDENTIALS) continue;
            if (in.empty()) return true;
        }
    }
    bool Decrypt() {     // 'in' -> 'out'
        while (!in.empty()) {
            SecBuffer b[4] = { { (ULONG)in.size(), SECBUFFER_DATA, in.data() }, {0,SECBUFFER_EMPTY,0}, {0,SECBUFFER_EMPTY,0}, {0,SECBUFFER_EMPTY,0} };
            SecBufferDesc d = { SECBUFFER_VERSION, 4, b };
            SECURITY_STATUS r = DecryptMessage(&ctx, &d, 0, nullptr);
            if (r == SEC_E_INCOMPLETE_MESSAGE) return true;
            if (r != SEC_E_OK) { lastStatus = r; return false; }
            std::string extra;
            for (auto& x : b) {
                if (x.BufferType == SECBUFFER_DATA) out.append((char*)x.pvBuffer, x.cbBuffer);
                else if (x.BufferType == SECBUFFER_EXTRA) extra.assign((char*)x.pvBuffer, x.cbBuffer);
            }
            in = extra;
        }
        return true;
    }
    std::string Enc(const std::string& m) {
        std::string r;
        for (size_t o = 0; o < m.size(); o += sz.cbMaximumMessage) {
            size_t n = (std::min)((size_t)(m.size() - o), (size_t)sz.cbMaximumMessage);
            std::string buf(sz.cbHeader + n + sz.cbTrailer, '\0'); memcpy(&buf[sz.cbHeader], m.data() + o, n);
            SecBuffer b[4] = { { sz.cbHeader, SECBUFFER_STREAM_HEADER, &buf[0] }, { (ULONG)n, SECBUFFER_DATA, &buf[sz.cbHeader] },
                               { sz.cbTrailer, SECBUFFER_STREAM_TRAILER, &buf[sz.cbHeader + n] }, { 0, SECBUFFER_EMPTY, nullptr } };
            SecBufferDesc d = { SECBUFFER_VERSION, 4, b };
            if (EncryptMessage(&ctx, 0, &d, 0) != SEC_E_OK) return "";
            r.append(buf.data(), b[0].cbBuffer + b[1].cbBuffer + b[2].cbBuffer);
        }
        return r;
    }
};

// ---------------- Connect / options dialog (template built in memory, no .rc) ----------------
enum { IDM_CONNECT = 9001, IDM_DISCONNECT, IDM_CASCADE, IDM_TILE, IDM_EXIT, IDM_SWTOP, IDM_SWBOTTOM, IDM_FONT, IDM_SERVERS, IDM_CHANFAVS, IDM_ABOUT, IDM_ALIASES, IDM_COLORS, IDM_LOGGING, IDM_ONLINETIMER, IDM_IDENTD, IDM_TRAY, IDM_TIPS, IDM_POPEDIT0, IDM_POPEDIT1, IDM_POPEDIT2, IDM_POPEDIT3, IDM_POPEDIT4,
       IDC_HOST = 101, IDC_PORT, IDC_NICK, IDC_USER, IDC_REAL, IDC_PASS, IDC_JOIN, IDC_TLS, IDC_LAX };
struct Opts {
    CString host = L"irc.libera.chat", nick = L"YourNickname", user = L"irc", real = L"IRC user", pass, autojoin;
    int port = 6667; BOOL tls = FALSE, lax = FALSE;
};
struct Bookmark { CString name; Opts o; };   // one saved entry in the server list (servers.ini); password is never saved
class CConnDlg : public CDialog {
    Opts& o; std::vector<WORD> t; int cnt = 0;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
    void Row(int y, const wchar_t* lbl, WORD id, DWORD extra = 0) {
        Item(SS_LEFT, 8, y + 2, 60, 10, 0xFFFF, 0x0082, lbl);
        Item(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | extra, 72, y, 150, 12, id, 0x0081, L"");
    }
public:
    CConnDlg(Opts& op, CWnd* parent) : o(op) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(232); t.push_back(184);   // cdit, x, y, cx, cy
        t.push_back(0); t.push_back(0); S(L"Connect to IRC server"); t.push_back(9); S(DEFAULT_FONT); //was Segoe UI
        Row(6, L"Server", IDC_HOST); Row(22, L"Port", IDC_PORT, ES_NUMBER); Row(38, L"Nickname", IDC_NICK);
        Row(54, L"User name", IDC_USER); Row(70, L"Real name", IDC_REAL); Row(86, L"Password", IDC_PASS, ES_PASSWORD);
        Row(102, L"Auto-join", IDC_JOIN);
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 72, 120, 150, 10, IDC_TLS, 0x0080, L"Use TLS (SSL) encryption");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 72, 134, 150, 10, IDC_LAX, 0x0080, L"Accept invalid certificates");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 110, 160, 50, 14, IDOK, 0x0080, L"Connect");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 166, 160, 50, 14, IDCANCEL, 0x0080, L"Cancel");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetDlgItemText(IDC_HOST, o.host); SetDlgItemInt(IDC_PORT, o.port); SetDlgItemText(IDC_NICK, o.nick);
        SetDlgItemText(IDC_USER, o.user); SetDlgItemText(IDC_REAL, o.real); SetDlgItemText(IDC_PASS, o.pass);
        SetDlgItemText(IDC_JOIN, o.autojoin); CheckDlgButton(IDC_TLS, o.tls); CheckDlgButton(IDC_LAX, o.lax);
        return TRUE;
    }
    // Ticking/unticking TLS flips the port between the plaintext and TLS defaults, unless the user
    // has already typed something else — connecting TLS to a plaintext port is the #1 cause of "SSL doesn't work".
    afx_msg void OnTlsClick() {
        int p = GetDlgItemInt(IDC_PORT); bool on = IsDlgButtonChecked(IDC_TLS) != 0;
        if (p == 6667 || p == 6697) SetDlgItemInt(IDC_PORT, on ? 6697 : 6667);
    }
    DECLARE_MESSAGE_MAP()
    void OnOK() override {
        GetDlgItemText(IDC_HOST, o.host); o.host.Trim(); if (o.host.IsEmpty()) return;
        o.port = GetDlgItemInt(IDC_PORT); GetDlgItemText(IDC_NICK, o.nick); GetDlgItemText(IDC_USER, o.user);
        GetDlgItemText(IDC_REAL, o.real); GetDlgItemText(IDC_PASS, o.pass); GetDlgItemText(IDC_JOIN, o.autojoin);
        o.tls = IsDlgButtonChecked(IDC_TLS); o.lax = IsDlgButtonChecked(IDC_LAX);
        if (o.port <= 0 || o.port > 65535) o.port = o.tls ? 6697 : 6667;
        if (o.nick.IsEmpty()) o.nick = L"IRCClient";
        if (o.user.IsEmpty()) o.user = L"irc";
        CDialog::OnOK();
    }
};
BEGIN_MESSAGE_MAP(CConnDlg, CDialog)
    ON_BN_CLICKED(IDC_TLS, OnTlsClick)
END_MESSAGE_MAP()

// ---------------- One-line text prompt (used for "Notice") ----------------
class CPromptDlg : public CDialog {
    CString& val; CString title, label; std::vector<WORD> t; int cnt = 0;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
public:
    CPromptDlg(CString& v, CString ttl, CString lbl, CWnd* parent) : val(v), title(ttl), label(lbl) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(220); t.push_back(66);
        t.push_back(0); t.push_back(0); S(title); t.push_back(9); S(DEFAULT_FONT); //was Segoe UI
        Item(SS_LEFT, 8, 8, 204, 10, 0xFFFF, 0x0082, label);
        Item(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 8, 20, 204, 12, 101, 0x0081, L"");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 100, 40, 50, 14, IDOK, 0x0080, L"OK");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 156, 40, 50, 14, IDCANCEL, 0x0080, L"Cancel");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override { CDialog::OnInitDialog(); SetDlgItemText(101, val); return TRUE; }
    void OnOK() override { GetDlgItemText(101, val); CDialog::OnOK(); }
};

// ---------------- Server List: saved bookmarks (host/port/nick/etc.), stored in servers.ini ----------------
enum { IDC_SLIST = 201, IDC_SL_CONNECT = 210, IDC_SL_NEW, IDC_SL_EDIT, IDC_SL_DELETE };
class CServerListDlg : public CDialog {
    std::vector<Bookmark>& bm; std::vector<WORD> t; int cnt = 0; CListBox m_list;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
    void Refill() {
        m_list.ResetContent();
        for (auto& e : bm) { CString p; p.Format(L"%d", e.o.port); m_list.AddString(e.name + L"  (" + e.o.host + L":" + p + (e.o.tls ? L", TLS)" : L")")); }
    }
public:
    int connectIdx = -1;
    CServerListDlg(std::vector<Bookmark>& b, CWnd* parent) : bm(b) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(254); t.push_back(156);
        t.push_back(0); t.push_back(0); S(L"Server List"); t.push_back(9); S(DEFAULT_FONT); //was Segoe UI
        Item(LBS_NOTIFY | LBS_HASSTRINGS | WS_VSCROLL | WS_BORDER | WS_TABSTOP, 6, 8, 242, 118, IDC_SLIST, 0x0083, L"");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 6, 132, 48, 16, IDC_SL_CONNECT, 0x0080, L"Connect");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 58, 132, 44, 16, IDC_SL_NEW, 0x0080, L"New...");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 106, 132, 44, 16, IDC_SL_EDIT, 0x0080, L"Edit...");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 154, 132, 50, 16, IDC_SL_DELETE, 0x0080, L"Delete");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 208, 132, 40, 16, IDCANCEL, 0x0080, L"Close");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override { CDialog::OnInitDialog(); m_list.SubclassDlgItem(IDC_SLIST, this); Refill(); return TRUE; }
    afx_msg void OnConnectBtn() { int i = m_list.GetCurSel(); if (i >= 0) { connectIdx = i; CDialog::OnOK(); } }
    afx_msg void OnNewBtn() {
        Opts o; CConnDlg d(o, this);
        if (d.DoModal() != IDOK) return;
        CString name = o.host; CPromptDlg pd(name, L"Server List", L"Name for this entry:", this);
        if (pd.DoModal() != IDOK || name.IsEmpty()) return;
        Bookmark e; e.name = name; e.o = o; bm.push_back(e); Refill();
    }
    afx_msg void OnEditBtn() {
        int i = m_list.GetCurSel(); if (i < 0) return;
        CConnDlg d(bm[i].o, this);
        if (d.DoModal() == IDOK) Refill();
    }
    afx_msg void OnDeleteBtn() {
        int i = m_list.GetCurSel(); if (i < 0) return;
        if (AfxMessageBox(L"Delete this server entry?", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        bm.erase(bm.begin() + i); Refill();
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CServerListDlg, CDialog)
    ON_BN_CLICKED(IDC_SL_CONNECT, OnConnectBtn) 
    ON_BN_CLICKED(IDC_SL_NEW, OnNewBtn)
    ON_BN_CLICKED(IDC_SL_EDIT, OnEditBtn) 
    ON_BN_CLICKED(IDC_SL_DELETE, OnDeleteBtn)
END_MESSAGE_MAP()

// ---------------- Channel Favorites: bookmarked channels, stored in channels.ini ----------------
struct ChanFav { CString chan, key, net; };   // net is just a display hint (where it was added from); joining always uses the active connection

struct Net;   // full definition comes much later in the file; PlayItem only needs the pointer, so a forward declaration is enough here
// One queued /play request: a set of lines to send out one at a time on a timer. See CMainFrame::CmdPlay / PlayTick.
struct PlayItem {
    Net* net = nullptr;
    CString target, alias, fname, topic;
    int delay = 1000;
    bool echo = false, asCmd = false, notice = false, clipTemp = false;
    std::vector<CString> lines;
    size_t pos = 0;
    ULONGLONG dueAt = 0;   // GetTickCount64() value at which the next line may send; 0 = not yet scheduled (send immediately)
    CString Status() const { return pos == 0 ? CString(L"waiting") : (pos >= lines.size() ? CString(L"done") : CString(L"running")); }
};
// Net+window-name are stored rather than a CChatWnd* directly, resolved fresh via Find() at fire time, since the
// window could be closed while the timer is still running (the same reasoning as PlayItem's net pointer, but a
// window is far more likely to be closed mid-flight than a network is to be destroyed).
struct SoundChannel {   // one of wave/midi/song; see /splay, /vol, $vol, $inwave/$inmidi/$insong
    CString alias;       // MCI device alias for this channel
    CString curFile;
    bool open = false, playing = false, paused = false;
    std::vector<std::pair<CString, int>> queue;   // -q queued (file, startPosMs) pairs, played in order once the current sound ends
};
struct TipInfo {   // see /tips, /tip, $tips, $tip -- a queued balloon tip, either from an automatic event or scripted via $tip()
    CString name, title, text;
    int delaySec = 10;    // -1 = permanent (stays until closed or dismissed)
    CString iconFn; int iconPos = 0;   // accepted for $tip() compatibility; native balloon tips can't render an arbitrary file's icon, only a fixed info/warning/error glyph -- see ShowTipBalloon
    CString alias;         // run (via RunScript) if the tip is clicked without Shift held
    int wid = 0;           // the owning window's CChatWnd::m_seq; 0 = none/unassigned
    ULONGLONG shownAt = 0; // 0 = still queued, not yet shown
    int seq = 0;           // creation order, for $tip(N)
};
// ---------------- /timer: repeating (or one-shot) scheduled commands ----------------
struct TimerInfo {
    CString name;                 // "1", "2", ... or a custom name; shown/matched without the leading "/timer"
    Net* net = nullptr; CString winName;
    bool offline = false;         // -o: keeps running across a disconnect (default is "online": stops when net disconnects, unless net was null to begin with)
    bool msMode = false;          // -m or -h: the given interval is milliseconds, not seconds
    bool catchUp = false;         // -c: if a tick falls behind, fire repeatedly (roughly) until caught up, instead of just resyncing to now
    bool dynAssoc = false;        // -i: accepted, not actually implemented (no dynamic re-association to a different connection)
    bool paused = false;          // -p: skips firing, countdown keeps going
    bool haltCountdown = false;   // -P: skips firing AND freezes the countdown
    int totalReps = 0;            // 0 = infinite
    int repsLeft = 0;
    double intervalSec = 0;
    CString command;              // with any $!identifier references already evaluated once, at creation time
    ULONGLONG nextFire = 0;       // GetTickCount64() value of the next scheduled fire
};

// ---------------- /dns: background-threaded resolution, one request in flight at a time ----------------
// One queued /dns request. A nickname stays "isNickname" until a USERHOST reply fills in resolvedHost (see OnLine's
// "302" handling); only then does it actually get resolved. See CMainFrame::CmdDns / StartNextDnsIfIdle.
struct DnsRequest {
    int id = 0;
    Net* net = nullptr; CString winName;
    CString query, resolvedHost, nsServer;
    bool ipv4 = true, ipv6 = true, hostForce = false, multi = false, isNickname = false, reverse = false;
    CString status = L"queued";   // queued | waiting for userhost | resolving | done | error
    std::vector<CString> addrs;
    std::vector<std::pair<CString, CString>> records;   // -m: (type, value) pairs, e.g. ("MX", "10 mail.example.com")
    CString error;
};
// What actually crosses the thread boundary: plain std::wstring/std::vector only (no CString/MFC heap assumptions
// across threads). The worker fills in the result fields, then posts this pointer back; the main thread owns
// deleting it afterward.
// ---------------- Identd server: a minimal RFC 1413 responder, always answering with the configured userid/system ----------------
// Like mIRC's own, this doesn't actually look up which local process owns the queried ports -- it just answers every
// query with the configured identity, which is all a real identd check from an IRC server is looking for anyway.
struct IdentdState {
    std::atomic<bool> stop{false}, running{false};
    std::wstring userId, system;
    int port = 113;
    HWND hwnd = nullptr;
};
static std::string WToA(const std::wstring& s) {   // ASCII-only narrow conversion: identd's userid/system fields are always plain ASCII (RFC 1413 / mIRC's own field rules), so a per-character truncating cast is correct here, not lossy
    std::string out; out.reserve(s.size());
    for (wchar_t c : s) out.push_back(static_cast<char>(c));
    return out;
}
static unsigned __stdcall IdentdThreadProc(void* p) {
    std::shared_ptr<IdentdState> state = *(std::shared_ptr<IdentdState>*)p; delete (std::shared_ptr<IdentdState>*)p;
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) { state->running = false; return 0; }
    BOOL reuse = TRUE; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    sockaddr_in addr = {}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons((u_short)state->port);
    if (bind(ls, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR || listen(ls, 5) == SOCKET_ERROR) { closesocket(ls); state->running = false; return 0; }
    state->running = true;
    while (!state->stop) {
        fd_set fds; FD_ZERO(&fds); FD_SET(ls, &fds);
        timeval tv{ 1, 0 };
        if (select(0, &fds, nullptr, nullptr, &tv) <= 0) continue;
        sockaddr_in peer = {}; int peerLen = sizeof(peer);
        SOCKET c = accept(ls, (sockaddr*)&peer, &peerLen);
        if (c == INVALID_SOCKET) continue;
        DWORD rcvTimeoutMs = 5000; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (char*)&rcvTimeoutMs, sizeof(rcvTimeoutMs));   // Windows wants milliseconds here, not a timeval like POSIX does
        char buf[256] = {}; int n = recv(c, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            std::string line(buf, n);
            size_t comma = line.find(',');
            if (comma != std::string::npos) {
                auto trim = [](std::string s) { size_t a = 0, b = s.size(); while (a < b && isspace((unsigned char)s[a])) a++; while (b > a && isspace((unsigned char)s[b - 1])) b--; return s.substr(a, b - a); };
                std::string p1 = trim(line.substr(0, comma)), p2 = trim(line.substr(comma + 1));
                std::string userIdA = WToA(state->userId), sysA = WToA(state->system);
                std::string reply = p1 + " , " + p2 + " : USERID : " + sysA + " : " + userIdA + "\r\n";
                send(c, reply.c_str(), (int)reply.size(), 0);
                wchar_t ipbuf[64] = {}; InetNtopW(AF_INET, &peer.sin_addr, ipbuf, 64);
                auto* notice = new std::pair<std::wstring, std::wstring>(ipbuf, std::wstring(reply.begin(), reply.end()));
                ::PostMessage(state->hwnd, WM_APP + 51, 0, (LPARAM)notice);
            }
        }
        closesocket(c);
    }
    closesocket(ls); state->running = false;
    return 0;
}

struct DnsJob {
    int id = 0; HWND hwnd = nullptr;
    std::wstring query, nsServer;
    bool ipv4 = true, ipv6 = true, multi = false, reverse = false;
    std::vector<std::wstring> addrs;
    std::vector<std::pair<std::wstring, std::wstring>> records;
    std::wstring error;
};
static std::wstring DnsPtrName(const std::wstring& ip) {   // builds the in-addr.arpa / ip6.arpa query name for a PTR lookup
    IN_ADDR a4;
    if (InetPtonW(AF_INET, ip.c_str(), &a4) == 1) {
        BYTE* b = (BYTE*)&a4.S_un.S_addr; wchar_t buf[64];
        swprintf_s(buf, L"%d.%d.%d.%d.in-addr.arpa", b[3], b[2], b[1], b[0]);
        return buf;
    }
    IN6_ADDR a6;
    if (InetPtonW(AF_INET6, ip.c_str(), &a6) == 1) {
        std::wstring s;
        for (int i = 15; i >= 0; i--) { wchar_t hex[8]; swprintf_s(hex, L"%x.%x.", a6.u.Byte[i] & 0xF, a6.u.Byte[i] >> 4); s += hex; }
        return s + L"ip6.arpa";
    }
    return ip;   // shouldn't happen: caller only builds a PTR name once it's already confirmed this is a literal IP
}
static PIP4_ARRAY DnsServerArray(const std::wstring& ns, IP4_ARRAY& storage) {   // DnsQuery_W's custom-server option is IPv4-only (a limitation of this legacy API)
    if (ns.empty()) return nullptr;
    IN_ADDR a; if (InetPtonW(AF_INET, ns.c_str(), &a) != 1) return nullptr;
    storage.AddrCount = 1; storage.AddrArray[0] = a.S_un.S_addr; return &storage;
}
static void DnsCollect(PCWSTR name, WORD type, PIP4_ARRAY srv, std::vector<std::wstring>* addrs, std::vector<std::pair<std::wstring, std::wstring>>* recs, const wchar_t* typeName) {
    PDNS_RECORD rec = nullptr;
    if (DnsQuery_W(name, type, DNS_QUERY_STANDARD, srv, &rec, nullptr) != 0 || !rec) return;
    for (PDNS_RECORD r = rec; r; r = r->pNext) {
        if (r->wType != type) continue;
        std::wstring val;
        wchar_t buf[128];
        switch (type) {
            case DNS_TYPE_A: { IN_ADDR a; a.S_un.S_addr = r->Data.A.IpAddress; InetNtopW(AF_INET, &a, buf, 128); val = buf; break; }
            case DNS_TYPE_AAAA: InetNtopW(AF_INET6, &r->Data.AAAA.Ip6Address, buf, 128); val = buf; break;
            case DNS_TYPE_PTR: val = r->Data.PTR.pNameHost; break;
            case DNS_TYPE_NS: val = r->Data.NS.pNameHost; break;
            case DNS_TYPE_MX: swprintf_s(buf, L"%d ", r->Data.MX.wPreference); val = std::wstring(buf) + r->Data.MX.pNameExchange; break;
            case DNS_TYPE_SOA: val = std::wstring(r->Data.SOA.pNamePrimaryServer) + L" " + r->Data.SOA.pNameAdministrator; break;
            case DNS_TYPE_SRV: swprintf_s(buf, L"%d %d %d ", r->Data.SRV.wPriority, r->Data.SRV.wWeight, r->Data.SRV.wPort); val = std::wstring(buf) + r->Data.SRV.pNameTarget; break;
            case DNS_TYPE_TEXT: for (DWORD i = 0; i < r->Data.TXT.dwStringCount; i++) { if (i) val += L" "; val += r->Data.TXT.pStringArray[i]; } break;
        }
        if (addrs) addrs->push_back(val);
        if (recs) recs->push_back({ typeName, val });
    }
    DnsRecordListFree(rec, DnsFreeRecordList);
}
static unsigned __stdcall DnsWorkerProc(void* p) {
    std::unique_ptr<DnsJob> j((DnsJob*)p);
    IP4_ARRAY srvStorage; PIP4_ARRAY srv = DnsServerArray(j->nsServer, srvStorage);
    if (j->multi) {
        static const struct { const wchar_t* name; WORD type; } kTypes[] = {
            { L"A", DNS_TYPE_A }, { L"AAAA", DNS_TYPE_AAAA }, { L"NS", DNS_TYPE_NS }, { L"MX", DNS_TYPE_MX },
            { L"SOA", DNS_TYPE_SOA }, { L"SRV", DNS_TYPE_SRV }, { L"TXT", DNS_TYPE_TEXT }
        };
        for (auto& kt : kTypes) DnsCollect(j->query.c_str(), kt.type, srv, nullptr, &j->records, kt.name);
        if (j->records.empty()) j->error = L"No records found.";
    } else if (j->reverse) {
        std::wstring ptr = DnsPtrName(j->query);
        DnsCollect(ptr.c_str(), DNS_TYPE_PTR, srv, &j->addrs, nullptr, L"PTR");
        if (j->addrs.empty()) j->error = L"Couldn't resolve.";
    } else {
        if (j->ipv4) DnsCollect(j->query.c_str(), DNS_TYPE_A, srv, &j->addrs, nullptr, L"A");
        if (j->ipv6) DnsCollect(j->query.c_str(), DNS_TYPE_AAAA, srv, &j->addrs, nullptr, L"AAAA");
        if (j->addrs.empty()) j->error = L"Couldn't resolve.";
    }
    HWND hwnd = j->hwnd; int id = j->id;
    ::PostMessage(hwnd, WM_APP + 50, (WPARAM)id, (LPARAM)j.release());   // ownership passes to the main thread, which deletes it after reading the results
    return 0;
}

// A named set of colors (the Colors dialog): 7 text colors for message types, plus 3 background colors that apply to
// the chat log, the input box, and the nick list (CLR_NONE for any of the ten means "use the system default").
struct ColorScheme {
    CString name;
    COLORREF normal = RGB(0,0,0), 
             ctcp = RGB(255, 0, 0),
             highlight = RGB(127, 0, 0),
             invite = RGB(0, 147, 0),
             join = RGB(0, 147, 0),
             part = RGB(0, 147, 0),
             quit = RGB(0, 0, 127),
             mode = RGB(0, 147, 0),
             topic = RGB(0, 147, 0),
             kick = RGB(0, 147, 0),
             nickname = RGB(0, 147, 0),
             own = RGB(0,0,0), 
             notice = RGB(200,110,0), 
             other = RGB(156, 0, 156),
             info = RGB(0,0,127), 
             info2 = RGB(0, 147, 0),
             action = RGB(150,0,150),
             wallops = RGB(127, 0, 0),
             whois = RGB(0, 0, 0);
    COLORREF chatBg = CLR_NONE, editBg = CLR_NONE, nickBg = CLR_NONE;
};
enum { IDC_FLIST = 301, IDC_FL_JOIN = 310, IDC_FL_NEW, IDC_FL_DELETE };
class CFavDlg : public CDialog {
    std::vector<ChanFav>& fv; std::vector<WORD> t; int cnt = 0; CListBox m_list;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
    void Refill() {
        m_list.ResetContent();
        //for (auto& e : fv) m_list.AddString(e.chan + (e.key.IsEmpty() ? CString() : L"  (key set)") + (e.net.IsEmpty() ? CString() : L"  — " + e.net));
        for (auto& e : fv)
        {
            m_list.AddString(e.chan +
                (e.key.IsEmpty() ? CString() : CString(L"  (key set)")) +
                (e.net.IsEmpty() ? CString() : L"  — " + e.net));
        }
    }
public:
    int joinIdx = -1;
    CFavDlg(std::vector<ChanFav>& f, CWnd* parent) : fv(f) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(220); t.push_back(150);
        t.push_back(0); t.push_back(0); S(L"Channel Favorites"); t.push_back(9); S(DEFAULT_FONT);  //was Segue UI
        Item(LBS_NOTIFY | LBS_HASSTRINGS | WS_VSCROLL | WS_BORDER | WS_TABSTOP, 6, 8, 208, 112, IDC_FLIST, 0x0083, L"");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 6, 126, 48, 16, IDC_FL_JOIN, 0x0080, L"Join");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 58, 126, 44, 16, IDC_FL_NEW, 0x0080, L"New...");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 106, 126, 50, 16, IDC_FL_DELETE, 0x0080, L"Delete");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 164, 126, 50, 16, IDCANCEL, 0x0080, L"Close");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override { CDialog::OnInitDialog(); m_list.SubclassDlgItem(IDC_FLIST, this); Refill(); return TRUE; }
    afx_msg void OnJoinBtn() { int i = m_list.GetCurSel(); if (i >= 0) { joinIdx = i; CDialog::OnOK(); } }
    afx_msg void OnDblClick() { OnJoinBtn(); }
    afx_msg void OnNewBtn() {
        CString chan = L"#";
        CPromptDlg d(chan, L"Channel Favorites", L"Channel name:", this);
        if (d.DoModal() != IDOK || chan.IsEmpty()) return;
        chan.Trim(); if (chan[0] != L'#' && chan[0] != L'&') chan = L"#" + chan;
        CString key;
        CPromptDlg kd(key, L"Channel Favorites", L"Key (leave blank if none):", this);
        kd.DoModal();   // optional; proceed either way
        ChanFav e; e.chan = chan; e.key = key; fv.push_back(e); Refill();
    }
    afx_msg void OnDeleteBtn() {
        int i = m_list.GetCurSel(); if (i < 0) return;
        if (AfxMessageBox(L"Remove this channel from favorites?", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        fv.erase(fv.begin() + i); Refill();
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CFavDlg, CDialog)
    ON_BN_CLICKED(IDC_FL_JOIN, OnJoinBtn) 
    ON_BN_CLICKED(IDC_FL_NEW, OnNewBtn) 
    ON_BN_CLICKED(IDC_FL_DELETE, OnDeleteBtn)
    ON_LBN_DBLCLK(IDC_FLIST, OnDblClick)
END_MESSAGE_MAP()

// ---------------- About dialog: app icon + banner image ----------------
// ---------------- A CStatic that reports right-clicks (used for the About banner's "change image") ----------------
class CClickableStatic : public CStatic {
public:
    std::function<void(CPoint)> onRClick;
protected:
    afx_msg void OnRButtonUp(UINT, CPoint p) { CPoint sp = p; ClientToScreen(&sp); if (onRClick) onRClick(sp); }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CClickableStatic, CStatic)
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

// A small filled rectangle that paints itself in whatever color it's set to and reports a left-click; used by the
// Colors dialog for its Background / Editbox / Nicklist swatches.
class CColorSwatch : public CStatic {
public:
    COLORREF color = RGB(255,255,255);
    std::function<void()> onClick, onClear;   // left click: choose a color. right click: reset to "use the default" (CLR_NONE)
protected:
    afx_msg void OnPaint() {
        CPaintDC dc(this); CRect r; GetClientRect(r);
        dc.FillSolidRect(r, color);
        dc.Draw3dRect(r, ::GetSysColor(COLOR_BTNSHADOW), ::GetSysColor(COLOR_BTNHIGHLIGHT));
    }
    afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    afx_msg void OnLButtonUp(UINT, CPoint) { if (onClick) onClick(); }
    afx_msg void OnRButtonUp(UINT, CPoint) { if (onClear) onClear(); }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CColorSwatch, CStatic)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

// The clickable grid itself: 0-15 as two rows of larger cells (the "basic" set, as mIRC shows them), then 16-98 as
// twelve columns of smaller cells. Left-click picks a foreground color; right-click picks a background color (added
// as ",NN" onto whatever's already there, since mIRC requires a foreground before a background is meaningful).
class CColorGridCtrl : public CWnd {
    static const int BW = 36, BH = 28, EW = 24, EH = 20, COLS = 12, GAP = 56 + 4;   // basic cell size, extended cell size, extended columns, extended area's y start
    static CRect CellRect(int idx) {
        if (idx < 16) { int col = idx % 8, row = idx / 8; return CRect(col * BW, row * BH, col * BW + BW - 2, row * BH + BH - 2); }
        int k = idx - 16, col = k % COLS, row = k / COLS;
        return CRect(col * EW, GAP + row * EH, col * EW + EW - 2, GAP + row * EH + EH - 2);
    }
    static int HitTest(CPoint p) {
        if (p.y < 2 * BH) {   // inside the two basic-color rows
            int col = p.x / BW, row = p.y / BH;
            if (col >= 0 && col < 8 && row >= 0 && row < 2) { int idx = row * 8 + col; return idx < 16 ? idx : -1; }
        }
        int ey = p.y - GAP; if (ey < 0) return -1;
        int col = p.x / EW, row = ey / EH;
        if (col < 0 || col >= COLS || row < 0) return -1;
        int idx = 16 + row * COLS + col;
        return idx <= 98 ? idx : -1;
    }
public:
    std::function<void(int, bool)> onPick;   // (color index 0-98, true if right-clicked = background)
    static CSize Extent() { CRect last = CellRect(98); return CSize(COLS * EW, last.bottom + 2); }
    BOOL Create(CWnd* parent, UINT id) {
        CSize sz = Extent();
        return CWnd::Create(AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_3DFACE + 1)), nullptr,
                            WS_CHILD | WS_VISIBLE, CRect(CPoint(4, 4), sz), parent, id);
    }
protected:
    afx_msg void OnPaint() {
        CPaintDC dc(this);
        CFont f; f.CreatePointFont(80, DEFAULT_FONT); CFont* old = dc.SelectObject(&f);
        dc.SetBkMode(TRANSPARENT);
        for (int i = 0; i <= 98; i++) {
            CRect r = CellRect(i); COLORREF bg = kMircPalette[i];
            dc.FillSolidRect(r, bg);
            dc.Draw3dRect(r, ::GetSysColor(COLOR_BTNSHADOW), ::GetSysColor(COLOR_BTNHIGHLIGHT));
            double lum = 0.299 * GetRValue(bg) + 0.587 * GetGValue(bg) + 0.114 * GetBValue(bg);
            dc.SetTextColor(lum > 140 ? RGB(0,0,0) : RGB(255,255,255));
            CString n; n.Format(L"%d", i);
            dc.DrawText(n, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        dc.SelectObject(old);
    }
    afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    afx_msg void OnLButtonDown(UINT, CPoint p) { int i = HitTest(p); if (i >= 0 && onPick) onPick(i, false); }
    afx_msg void OnRButtonDown(UINT, CPoint p) { int i = HitTest(p); if (i >= 0 && onPick) onPick(i, true); }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CColorGridCtrl, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

// Ctrl+K's color picker: pick a color (or right-click for background), and it closes itself immediately -- no
// OK/Cancel needed, Esc still closes it without picking anything (CDialog's normal default).
class CColorPickerDlg : public CDialog {
    std::vector<WORD> t; int cnt = 0;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    CColorGridCtrl m_grid;
public:
    int picked = -1; bool pickedBg = false;
    CColorPickerDlg(CWnd* parent) {
        CSize sz = CColorGridCtrl::Extent();
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0);
        t.push_back((short)((sz.cx + 16) * 4 / 13)); t.push_back((short)((sz.cy + 30) * 8 / 13));   // pixels -> rough dialog units at this font
        t.push_back(0); t.push_back(0); S(L"Colors"); t.push_back(9); S(DEFAULT_FONT);
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        m_grid.Create(this, 1);
        m_grid.onPick = [this](int idx, bool bg) { picked = idx; pickedBg = bg; EndDialog(IDOK); };
        CRect cr; m_grid.GetWindowRect(cr); ScreenToClient(cr);
        CRect wr; GetWindowRect(wr); CRect cl; GetClientRect(cl);
        SetWindowPos(nullptr, 0, 0, cr.right + 8 + (wr.Width() - cl.Width()), cr.bottom + 8 + (wr.Height() - cl.Height()), SWP_NOMOVE | SWP_NOZORDER);
        return TRUE;
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CColorPickerDlg, CDialog)
END_MESSAGE_MAP()

// /playctrl: lists the current /play queue (see CMainFrame::CmdPlayCtrl). Remove takes out just the selected
// request; Stop All clears the whole queue and halts playback.
enum { IDC_PC_LIST = 621, IDC_PC_REMOVE, IDC_PC_STOPALL };
class CPlayCtrlDlg : public CDialog {
    std::vector<PlayItem>& q; std::vector<WORD> t; int cnt = 0; CListBox m_list;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
    void Refill() {
        m_list.ResetContent();
        for (auto& it : q) {
            CString s; s.Format(L"%s -> %s  (%d/%d)  %dms  [%s]", (LPCWSTR)it.fname, (LPCWSTR)it.target, (int)it.pos, (int)it.lines.size(), it.delay, (LPCWSTR)it.Status());
            m_list.AddString(s);
        }
        GetDlgItem(IDC_PC_REMOVE)->EnableWindow(m_list.GetCount() > 0);
    }
public:
    CPlayCtrlDlg(std::vector<PlayItem>& queue, CWnd* parent) : q(queue) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(280); t.push_back(140);
        t.push_back(0); t.push_back(0); S(L"Play Central"); t.push_back(9); S(DEFAULT_FONT);
        Item(LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_HSCROLL | WS_BORDER | WS_TABSTOP, 8, 8, 264, 100, IDC_PC_LIST, 0x0083, L"");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 8, 116, 60, 14, IDC_PC_REMOVE, 0x0080, L"Remove");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 72, 116, 60, 14, IDC_PC_STOPALL, 0x0080, L"Stop All");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 212, 116, 60, 14, IDCANCEL, 0x0080, L"Close");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override { CDialog::OnInitDialog(); m_list.SubclassDlgItem(IDC_PC_LIST, this); Refill(); return TRUE; }
    afx_msg void OnRemove() {
        int i = m_list.GetCurSel(); if (i < 0 || i >= (int)q.size()) return;
        if (q[i].clipTemp) ::DeleteFileW(q[i].fname);
        q.erase(q.begin() + i); Refill();
    }
    afx_msg void OnStopAll() {
        for (auto& it : q) if (it.clipTemp) ::DeleteFileW(it.fname);
        q.clear(); Refill();
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CPlayCtrlDlg, CDialog)
    ON_BN_CLICKED(IDC_PC_REMOVE, OnRemove) ON_BN_CLICKED(IDC_PC_STOPALL, OnStopAll)
END_MESSAGE_MAP()

class CAboutDlg : public CDialog {
    std::vector<WORD> t; int cnt = 0;
    CClickableStatic m_banner; 
    CBitmap m_aboutBmp;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
    // A STATIC control whose picture is an icon/bitmap RESOURCE, referenced by numeric id rather than a string —
    // the DLGITEMTEMPLATE "text" field can be the 0xFFFF-ordinal form here too, same trick used for control classes.
    void ItemRes(DWORD st, int x, int y, int cx, int cy, WORD id, WORD resId) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(0x0082);   // class = STATIC
        t.push_back(0xFFFF); t.push_back(resId);    // "text" = ordinal resource id, not a string
        t.push_back(0); ++cnt;
    }
public:
    CAboutDlg(CWnd* parent) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); 
        t.push_back(0); 
        t.push_back(0); 
        t.push_back(320);
        t.push_back(400);
        t.push_back(0); 
        t.push_back(0); 
        S(L"About IRC"); 
        t.push_back(9); 
        S(DEFAULT_FONT);
        ItemRes(SS_ICON, 10, 10, 24, 24, 500, 101);          // the app icon
        ItemRes(SS_BITMAP | SS_NOTIFY, 10, 40, 300, 300, 501, 103);  // banner image, moved/resized to fit inside the enlarged dialog
        Item(SS_LEFT, 10, 350, 300, 20, 0xFFFF, 0x0082, L"IRC a mIRC-style IRC client for Windows, built with MFC.");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 136, 374, 48, 16, IDOK, 0x0080, L"OK");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        m_banner.SubclassDlgItem(501, this);
        m_banner.onRClick = [this](CPoint pt) {
            //AfxMessageBox(L"Test.....");
            CMenu m; m.CreatePopupMenu();
            m.AppendMenu(MF_STRING, 1, L"Change Image...");
            SetForegroundWindow();   // required by Windows for the popup to reliably receive clicks at all
            int cmd = m.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
            PostMessage(WM_NULL, 0, 0);   // MSDN-documented pairing for the above
            if (cmd == 1) ChangeImage();
        };
        return TRUE;
    }
    void ChangeImage() {
        CFileDialog fd(TRUE, L"png", nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
            L"Image Files (*.bmp;*.jpg;*.jpeg;*.png;*.gif)|*.bmp;*.jpg;*.jpeg;*.png;*.gif|All Files (*.*)|*.*||", this);
        if (fd.DoModal() != IDOK) return;
        HBITMAP hb = LoadImageFileScaled(fd.GetPathName(), 300, 300);
        if (!hb) { AfxMessageBox(L"Couldn't load that image."); return; }
        m_aboutBmp.DeleteObject(); m_aboutBmp.Attach(hb);
        m_banner.SetBitmap((HBITMAP)m_aboutBmp);
    }
};

// ---------------- Socket: line-buffered, UTF-8, optional TLS ----------------
class CIrcSock : public CAsyncSocket {
public:
    std::function<void(int)> onConn;
    std::function<void(const CString&)> onLine;
    std::function<void()> onDrop;
    CStringA buf; CTls* tls = nullptr; std::string sendq;
    ~CIrcSock() { delete tls; }
    void Queue(const std::string& x) { sendq += x; Flush(); }
    void Write(const std::string& x) { Queue(tls && tls->ready ? tls->Enc(x) : x); }
    void Flush() {
        while (!sendq.empty()) {
            int n = CAsyncSocket::Send(sendq.data(), (int)sendq.size());
            if (n == SOCKET_ERROR) break;
            sendq.erase(0, n);
        }
    }
    void OnSend(int) override { Flush(); }
    // Replaces plain Create()+Connect(host,port): that pair resolves via MFC's legacy, IPv4-only path (gethostbyname /
    // inet_addr under the hood), which can't parse an IPv6 literal (e.g. "2001:6b0:78::30") at all and fails
    // immediately. GetAddrInfoW is the modern, dual-stack-aware resolver -- it handles IPv4 literals, IPv6 literals,
    // and regular hostnames (picking whichever family the OS resolves/prefers), and the socket is then (re)created
    // with the matching address family before connecting.
    bool ConnectSmart(const CString& host, UINT port) {
        ADDRINFOW hints = {}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
        CString portStr; portStr.Format(L"%u", port);
        PADDRINFOW result = nullptr;
        if (::GetAddrInfoW(host, portStr, &hints, &result) != 0 || !result) return false;
        if (m_hSocket != INVALID_SOCKET) Close();
        bool ok = false;
        SOCKET s = ::socket(result->ai_family, SOCK_STREAM, IPPROTO_TCP);   // created manually with the resolved family, since CAsyncSocket::Create itself has no family parameter to pick IPv6 with
        if (s != INVALID_SOCKET) {
            if (Attach(s, FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE)) {   // Attach wires up the same async notifications Create would have, and puts the socket in non-blocking mode as a side effect
                int rc = ::connect(m_hSocket, result->ai_addr, (int)result->ai_addrlen);
                ok = (rc == 0) || (::WSAGetLastError() == WSAEWOULDBLOCK);
            } else ::closesocket(s);
        }
        ::FreeAddrInfoW(result);
        return ok;
    }
    void OnConnect(int e) override {
        if (e || !tls) { if (onConn) onConn(e); return; }
        if (!tls->Handshake()) { if (onConn) onConn((int)tls->lastStatus); return; }
        Queue(tls->tosend); tls->tosend.clear();          // send ClientHello; onConn fires when TLS is ready
    }
    void OnReceive(int) override {
        char b[16384]; int n = Receive(b, sizeof b);
        if (n <= 0) return;
        std::string plain;
        if (tls) {
            tls->in.append(b, n);
            if (!tls->ready) {
                if (!tls->Handshake()) { Close(); if (onConn) onConn((int)tls->lastStatus); return; }
                Queue(tls->tosend); tls->tosend.clear();
                if (tls->ready && onConn) onConn(0);
            }
            if (tls->ready) {
                if (!tls->Decrypt()) { Close(); if (onDrop) onDrop(); return; }
                plain.swap(tls->out);
            }
        } else plain.assign(b, n);
        buf.Append(plain.data(), (int)plain.size());
        int i;
        while ((i = buf.Find('\n')) >= 0) {
            CStringA l = buf.Left(i); buf = buf.Mid(i + 1); l.TrimRight("\r");
            if (onLine) onLine(CString(CA2W(l, CP_UTF8)));
        }
    }
    void OnClose(int) override { Close(); if (onDrop) onDrop(); }
};

// ---------------- Chat log: single-click a #channel to join/open it ----------------
class CLogEdit : public CRichEditCtrl {
public:
    std::function<void(CString)> onLink;
    std::function<bool(CPoint)> onContext;   // right-click: return true if a popup menu was shown (otherwise the default edit menu appears)
    // Background color only -- an image here was tried and abandoned (see CMainFrame's Colors/background notes):
    // RichEdit's own internal painting actively fights anything external trying to draw its background, in ways
    // that never fully resolved. A plain color goes through RichEdit's own native mechanism instead (see
    // CChatWnd::ApplyColors' call to SetBackgroundColor), which doesn't have that problem.
    COLORREF bgColor = CLR_NONE;
protected:
    afx_msg void OnContextMenu(CWnd*, CPoint pt) {   // keyboard-invoked only (Shift+F10 / Menu key): those still arrive this way
        if (pt.x == -1 && pt.y == -1) GetCursorPos(&pt);
        if (onContext && onContext(pt)) return;
        Default();
    }
    // A real right-click is caught here directly rather than relying on Windows to synthesize WM_CONTEXTMENU
    // afterward, since that synthesis isn't reliably reaching this control. Handling the raw click ourselves
    // sidesteps that entirely.
    afx_msg void OnRButtonUp(UINT, CPoint p) {
        CPoint sp = p; ClientToScreen(&sp);
        if (!onContext || !onContext(sp)) Default();   // Default() here still lets Windows show its own Copy/Paste menu when we decline
    }
    // Shared by the click handler and the hover-cursor check below: the trimmed "word" at a given client point (the
    // same word-boundary logic either way, so a click and the hand cursor always agree on what counts as a link).
    CString WordAtPoint(CPoint p) {
        int idx = CharFromPos(p), li = LineFromChar(idx), st = LineIndex(li);
        CString ln; GetTextRange(st, st + LineLength(idx), ln);
        int q = idx - st, n = ln.GetLength(); if (q > n) q = n;
        int a = q, b = q;
        while (a > 0 && !iswspace(ln[a - 1])) a--;
        while (b < n && !iswspace(ln[b])) b++;
        CPoint pa = PosFromChar(st + a), pb = PosFromChar(st + b);
        if (p.x < pa.x || p.x > pb.x) return CString();   // clicked/hovered blank space, not the word itself
        CString w = ln.Mid(a, b - a); w.Trim(L",.;:!?()<>[]'\"");
        return w;
    }
    static bool IsUrlWord(const CString& w) {
        CString wl = w; wl.MakeLower();
        return wl.Left(7) == L"http://" || wl.Left(8) == L"https://" || wl.Left(6) == L"ftp://" || wl.Left(4) == L"www.";
    }
    bool IsLinkWord(const CString& w) const { return (w.GetLength() > 1 && (w[0] == L'#' || w[0] == L'&')) || IsUrlWord(w); }
    afx_msg void OnLButtonUp(UINT, CPoint p) {
        Default();
        long s = 0, e = 0; GetSel(s, e);
        if (s != e) { Copy(); return; }                         // a selection was just made: auto-copy it, like a Windows console window
        CString w = WordAtPoint(p);
        if (IsUrlWord(w)) {   // opened directly, independent of onLink: this is a generic action, not something that needs app-specific channel/nick context
            CString url = w;
            if (url.Left(4).CompareNoCase(L"www.") == 0) url = L"https://" + url;   // a bare "www." word needs a scheme before ShellExecute will treat it as a URL
            ::ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
        }
        else if (onLink && w.GetLength() > 1 && (w[0] == L'#' || w[0] == L'&')) onLink(w);
    }
    // Shows a hand cursor over a clickable #channel/&channel name, like mIRC (and every browser) does for a link --
    // this fires on essentially every mouse movement over the control, so it reuses WordAtPoint rather than anything
    // heavier.
    afx_msg BOOL OnSetCursor(CWnd* w, UINT nHitTest, UINT message) {
        if (nHitTest == HTCLIENT) {
            CPoint p; GetCursorPos(&p); ScreenToClient(&p);
            if (IsLinkWord(WordAtPoint(p))) { ::SetCursor(::LoadCursor(nullptr, IDC_HAND)); return TRUE; }
        }
        return CRichEditCtrl::OnSetCursor(w, nHitTest, message);
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CLogEdit, CRichEditCtrl)
    ON_WM_CONTEXTMENU()
    ON_WM_RBUTTONUP()
    ON_WM_LBUTTONUP()
    ON_WM_SETCURSOR()
END_MESSAGE_MAP()

// ---------------- MDI child: status / channel / query window ----------------
struct Net;   // forward decl: each chat window belongs to one network (see the Net struct, defined near CMainFrame)
class CListWnd;   // forward decl: the /list results window, defined further down

// ---------------- Nick list: right-click a nick for Whois / Query / Notice ----------------
class CNickList : public CListBox {
public:
    std::function<void(CString, CPoint)> onRClick;   // (the selected nicks, space separated with the clicked one first; screen point)
protected:
    afx_msg void OnRButtonDown(UINT, CPoint p) {
        BOOL outside = TRUE; int idx = ItemFromPoint(p, outside);
        if (idx < 0 || outside) return;
        if (!GetSel(idx)) { SelItemRange(FALSE, 0, GetCount() - 1); SetSel(idx, TRUE); }   // right-clicking a nick outside the selection selects just that one
        CString names, s; GetText(idx, s); names = Bare(s);
        for (int i = 0; i < GetCount(); i++) if (i != idx && GetSel(i) > 0) { GetText(i, s); names += L" " + Bare(s); }
        CPoint sp = p; ClientToScreen(&sp);
        if (onRClick) onRClick(names, sp);
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CNickList, CListBox)
    ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

class CChatWnd : public CMDIChildWnd {
public:
    CString m_name; bool m_chan, m_refresh = false; int m_act = 0, m_seq = 0;   // m_act: 0 none, 1 event, 2 message
    Net* net = nullptr;   // which network this window belongs to; set by the frame right after construction
    std::function<void(CChatWnd*, CString)> onInput;
    std::function<void(CChatWnd*)> onClose;
    std::function<void(CChatWnd*, CString)> onOpen;      // open/join a nick or #channel, on this window's network
    std::function<void(CChatWnd*, CString, CPoint)> onNickMenu;   // right-click nick(s) in the user list (the nicks, space separated)
    std::function<bool(CChatWnd*, CPoint)> onLogMenu;             // right-click in the chat log: true if a popup menu was shown
    std::function<void(const CString&)> onLog;                    // called with the plain (color-code-stripped, un-timestamped) text of each new line, for history logging
    int m_tsMode = -1;   // this window's /timestamp override: -1 = follow the global setting, 0 = off, 1 = on
    // ---- /window: custom @windows reuse this class (net stays null unless -i is used) ----
    bool m_custom = false;        // true for an @window created via /window
    bool m_hasEdit = true;        // false = no editbox row at all (mIRC's default for a new custom window, unless -e is given)
    bool m_cwListMode = false;    // -l: recorded for $window().lb; the display area itself is the same rich-text log either way (see the note in CmdWindow)
    bool m_cwSort = false;        // -s: keep m_cwLines sorted whenever it's modified
    int m_cwId = 0;               // stable id for $window(N) / $window().wid
    bool m_cwAnysc = false;       // -i was given (reported via $window().anysc; the actual dynamic re-association isn't implemented)
    CString m_cwDefCmd;           // the /command run when the user enters plain text (only reachable if m_hasEdit)
    std::vector<CString> m_cwPopup;                    // popup.txt's raw lines, parsed fresh each time the popup is shown
    std::vector<CString> m_cwLines; std::vector<COLORREF> m_cwColors;   // the line store behind /aline /cline /dline /iline /rline /sline
    int m_cwSelectedLine = -1;    // 1-based; -1 = none (see /sline, $sline) -- shown via the log's own text selection, since there's no separate listbox control
    void CwRebuild() {   // repaints the display from m_cwLines/m_cwColors after any line-store change
        m_out.SetWindowText(L"");
        for (size_t i = 0; i < m_cwLines.size(); i++) AddLine(m_cwLines[i], i < m_cwColors.size() ? m_cwColors[i] : cText, 0);
        if (m_cwSelectedLine >= 1 && m_cwSelectedLine <= (int)m_cwLines.size()) CwSelectLine(m_cwSelectedLine);
    }
    void CwSelectLine(int n) {   // best-effort "selection": highlights that line's text using the log's own text selection
        if (n < 1 || n > m_out.GetLineCount()) return;
        int st = m_out.LineIndex(n - 1); if (st < 0) return;
        int len = m_out.LineLength(st);
        m_out.SetSel(st, st + len);
    }
    std::function<bool()> tsEnabled;    // resolves m_tsMode against the global setting (see CMainFrame::Open)
    std::function<CString()> tsFormat;  // the current event timestamp format (e.g. "[HH:nn]"; a separating space is always added after it, see AddLine)
    CChatWnd(CString n, bool c) : m_name(n), m_chan(c) {}
    CString m_topicRaw; std::vector<CString> m_topicHist;   // the channel topic with its colour codes, and earlier topics (most recent first)
    bool LogHasSelection() { long s = 0, e = 0; m_out.GetSel(s, e); return s != e; }
    void LogCopy() { m_out.Copy(); }

    void Put(const CString& t, COLORREF fg, COLORREF bg, DWORD fx) {   // every new run also carries the current font explicitly
        m_out.SetSel(-1, -1);
        CHARFORMAT2 cf = {}; cf.cbSize = sizeof cf;
        cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT | CFM_FACE | CFM_SIZE;
        cf.crTextColor = fg;
        cf.dwEffects = fx | (m_baseBold ? CFE_BOLD : 0) | (m_baseItalic ? CFE_ITALIC : 0);
        if (bg == CLR_NONE) cf.dwEffects |= CFE_AUTOBACKCOLOR;   // no explicit mIRC background colour: leave it transparent so a background image shows through
        else { cf.dwMask |= CFM_BACKCOLOR; cf.crBackColor = bg; }
        cf.yHeight = m_fontTwips; wcsncpy_s(cf.szFaceName, m_face, LF_FACESIZE - 1);
        m_out.SetSelectionCharFormat(cf);
        m_out.ReplaceSel(t);
    }
    // Renders mIRC codes: ^B bold, ^C fg[,bg], ^E strikethrough, ^I italic, ^O reset, ^R reverse, ^_ underline
    // tsOverride: -1 = the window's normal /timestamp setting (every caller except /echo uses this), 0 = never
    // timestamp this line, 1 = reserved/unused for now. /echo's plain form passes 0 (mIRC never timestamps a bare
    // /echo); /echo -t passes -1, applying the normal on/off rule to that one line rather than forcing it on.
    void AddLine(CString s, COLORREF base, int tsOverride = -1) {
        bool showTs = tsOverride >= 0 ? (tsOverride != 0) : (tsEnabled ? tsEnabled() : true);
        CString ts = showTs ? FormatTimestamp(tsFormat ? tsFormat() : CString(L"[HH:nn]")) + L" " : CString();   // the space is always added here, regardless of whether the format string itself has one
        CString plain = ts, rawMsg;   // plain: what's shown (used for the #channel-underline pass below); rawMsg: undated, for logging
        long ls = 0, le = 0;
        m_out.SetSel(-1, -1); m_out.GetSel(ls, le);            // remember where this line starts
        if (showTs) Put(ts, base, CLR_NONE, 0);
        COLORREF fg = base, bg = CLR_NONE; DWORD fx = 0; CString seg; int n = s.GetLength();
        auto flush = [&] { if (!seg.IsEmpty()) { plain += seg; rawMsg += seg; Put(seg, fg, bg, fx); seg.Empty(); } };
        auto num = [&](int& i) { int v = -1; for (int k = 0; k < 2 && i + 1 < n && iswdigit(s[i + 1]); k++) v = (v < 0 ? 0 : v * 10) + s[++i] - L'0'; return v; };
        for (int i = 0; i < n; i++) {
            wchar_t c = s[i];
            if (c == 2) { flush(); fx ^= CFE_BOLD; }
            else if (c == 29) { flush(); fx ^= CFE_ITALIC; }
            else if (c == 30) { flush(); fx ^= CFE_STRIKEOUT; }
            else if (c == 31) { flush(); fx ^= CFE_UNDERLINE; }
            else if (c == 22) { flush(); COLORREF t = fg; fg = bg == CLR_NONE ? RGB(255,255,255) : bg; bg = t; }
            else if (c == 15) { flush(); fg = base; bg = CLR_NONE; fx = 0; }
            else if (c == 3) {
                flush(); int f = num(i);
                if (f < 0) { fg = base; bg = CLR_NONE; continue; }
                fg = MircColor(f);
                if (i + 2 < n && s[i + 1] == L',' && iswdigit(s[i + 2])) { i++; bg = MircColor(num(i)); }
            }
            else seg += c;
        }
        flush(); Put(L"\r\n", base, CLR_NONE, 0);
        if (onLog) onLog(rawMsg);
        for (int i = 0, n2 = plain.GetLength(); i < n2;) {     // underline #channel words
            if ((plain[i] == L'#' || plain[i] == L'&') && (i == 0 || iswspace(plain[i - 1]) || wcschr(L"([<,:", plain[i - 1]))) {
                int j = i + 1; while (j < n2 && !iswspace(plain[j]) && !wcschr(L",.;:!?)>]\"'", plain[j])) j++;
                if (j - i > 1) {
                    m_out.SetSel(ls + i, ls + j);
                    CHARFORMAT2 lf = {}; lf.cbSize = sizeof lf; lf.dwMask = CFM_COLOR | CFM_UNDERLINE;
                    lf.crTextColor = RGB(0, 0, 238); lf.dwEffects = CFE_UNDERLINE;
                    m_out.SetSelectionCharFormat(lf);
                }
                i = j;
            } else i++;
        }
        m_out.SetSel(-1, -1);
        m_out.SendMessage(WM_VSCROLL, SB_BOTTOM, 0);
    }
    void Clear() { m_out.SetWindowText(L""); }
    void SetTopic(const CString& t) {
        if (!m_chan) return;
        m_topic.SetWindowText(Strip(t));
        m_topicRaw = t;
        if (t.IsEmpty()) return;
        for (size_t i = 0; i < m_topicHist.size(); i++) if (m_topicHist[i] == t) { m_topicHist.erase(m_topicHist.begin() + i); break; }
        m_topicHist.insert(m_topicHist.begin(), t);
        if (m_topicHist.size() > 25) m_topicHist.pop_back();
    }
    void AddNick(const CString& n) { if (m_chan && !n.IsEmpty() && !HasNick(Bare(n))) m_nicks.AddString(n); }
    void ApplyFont(const LOGFONT& lf) {   // called once at creation and again whenever the user changes the font
        m_font.DeleteObject(); m_font.CreateFontIndirectW(&lf);
        wcsncpy_s(m_face, lf.lfFaceName, LF_FACESIZE - 1);
        m_baseBold = lf.lfWeight >= FW_BOLD; m_baseItalic = lf.lfItalic != 0;
        if (m_out.m_hWnd) {
            CClientDC dc(&m_out); m_fontTwips = -MulDiv(lf.lfHeight, 1440, dc.GetDeviceCaps(LOGPIXELSY));
            // A Rich Edit control mostly ignores WM_SETFONT (what CWnd::SetFont sends) once it has text, so
            // that alone won't restyle anything. Force the font onto every existing character explicitly instead.
            CHARFORMAT2 cf = {}; cf.cbSize = sizeof cf; cf.dwMask = CFM_FACE | CFM_SIZE;
            cf.yHeight = m_fontTwips; wcsncpy_s(cf.szFaceName, m_face, LF_FACESIZE - 1);
            long s = 0, e = 0; m_out.GetSel(s, e);
            m_out.SetSel(0, -1); m_out.SetSelectionCharFormat(cf);   // keeps each line's own color, only changes face/size
            m_out.SetSel(s, e);
        }
        if (m_in.m_hWnd) m_in.SetFont(&m_font);
        if (m_chan) { if (m_topic.m_hWnd) m_topic.SetFont(&m_font); if (m_nicks.m_hWnd) m_nicks.SetFont(&m_font); }
    }
    void ClearNicks() { if (m_chan) m_nicks.ResetContent(); }
    int NickCount() { return m_chan ? m_nicks.GetCount() : 0; }
    int FindNick(const CString& n) {
        if (!m_chan) return -1;
        for (int i = 0; i < m_nicks.GetCount(); i++) {
            CString s; m_nicks.GetText(i, s);
            if (Bare(s).CompareNoCase(n) == 0) return i;
        }
        return -1;
    }
    bool HasNick(const CString& n) { return FindNick(n) >= 0; }
    bool DelNick(const CString& n) { int i = FindNick(n); if (i < 0) return false; m_nicks.DeleteString(i); return true; }
    // (background image on the chat log itself was tried and abandoned -- see the notes on CLogEdit)
    // The chat background color (behind the log text, only visible where there's no image), and the editbox / nicklist
    // background colors; CLR_NONE means "use the system default" for that one. See CMainFrame::ApplyColorScheme.
    COLORREF m_editBg = CLR_NONE, m_nickBg = CLR_NONE;
    CBrush m_editBrush, m_nickBrush;
    void ApplyColors(COLORREF chatBg, COLORREF editBg, COLORREF nickBg) {
        m_out.bgColor = chatBg;
        // RichEdit's own native background-color mechanism handles this correctly (unlike trying to paint it
        // ourselves -- see the notes on CLogEdit and this app's Colors feature for why that was abandoned).
        if (m_out.m_hWnd) { m_out.SetBackgroundColor(chatBg == CLR_NONE, chatBg); m_out.Invalidate(); }
        m_editBg = editBg; if (m_editBrush.m_hObject) m_editBrush.DeleteObject();
        if (editBg != CLR_NONE) m_editBrush.CreateSolidBrush(editBg);
        m_nickBg = nickBg; if (m_nickBrush.m_hObject) m_nickBrush.DeleteObject();
        if (nickBg != CLR_NONE) m_nickBrush.CreateSolidBrush(nickBg);
        if (m_in.m_hWnd) m_in.Invalidate();
        if (m_nicks.m_hWnd) m_nicks.Invalidate();
    }

protected:
    long m_fontTwips = 200; wchar_t m_face[LF_FACESIZE] = DEFAULT_FONT; bool m_baseBold = false, m_baseItalic = false;
    std::vector<CString> m_hist; int m_histPos = -1;   // per-window input history; -1 = not currently browsing it
    CLogEdit m_out; CEdit m_in, m_topic; CNickList m_nicks; CFont m_font;

    afx_msg int OnCreate(LPCREATESTRUCT cs) {
        if (CMDIChildWnd::OnCreate(cs) == -1) return -1;
        CRect z(0, 0, 0, 0);
        m_out.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, z, this, 1);
        m_out.LimitText(0x7FFFFFF); m_out.onLink = [this](CString w) { if (onOpen) onOpen(this, w); };
        m_out.onContext = [this](CPoint pt) { return onLogMenu ? onLogMenu(this, pt) : false; };
        m_in.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, z, this, 2);
        m_font.CreatePointFont(100, DEFAULT_FONT);
        m_out.SetFont(&m_font); m_in.SetFont(&m_font);
        if (m_chan) {
            m_topic.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, z, this, 3);
            m_nicks.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_SORT | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_EXTENDEDSEL, z, this, 4);
            m_nicks.onRClick = [this](CString n, CPoint pt) { if (onNickMenu) onNickMenu(this, n, pt); };
            m_topic.SetFont(&m_font); m_nicks.SetFont(&m_font);
        }
        return 0;
    }
    afx_msg void OnSize(UINT t, int cx, int cy) {
        CMDIChildWnd::OnSize(t, cx, cy);
        if (!m_in.m_hWnd) return;
        int topicH = 22, inH = m_hasEdit ? 22 : 0, top = m_chan ? topicH : 0, nw = m_chan ? 140 : 0;
        if (m_chan) m_topic.MoveWindow(0, 0, cx, topicH);
        m_out.MoveWindow(0, top, cx - nw, cy - top - inH);
        if (m_chan) m_nicks.MoveWindow(cx - nw, top, nw, cy - top - inH);
        if (m_hasEdit) m_in.MoveWindow(0, cy - inH, cx, inH); else m_in.MoveWindow(0, cy, cx, 0);
    }
    afx_msg void OnNickDbl() {   // double-click a nick in the list -> open a query window
        int i = m_nicks.GetCaretIndex(); CString n;   // (GetCurSel doesn't work on a multiple-selection list)
        if (i >= 0 && onOpen) { m_nicks.GetText(i, n); onOpen(this, Bare(n)); }
    }
    afx_msg void OnSetFocus(CWnd*) { m_in.SetFocus(); }
    afx_msg HBRUSH OnCtlColor(CDC* dc, CWnd* w, UINT type) {
        HBRUSH br = CMDIChildWnd::OnCtlColor(dc, w, type);
        if (w->m_hWnd == m_in.m_hWnd && m_editBg != CLR_NONE) { dc->SetBkColor(m_editBg); return (HBRUSH)m_editBrush; }
        if (w->m_hWnd == m_nicks.m_hWnd && m_nickBg != CLR_NONE) { dc->SetBkColor(m_nickBg); return (HBRUSH)m_nickBrush; }
        return br;
    }
    // (the MDI-activate repaint timer that used to live here was specifically to fight the background-image race;
    // it's gone along with that feature, since a plain color via SetBackgroundColor doesn't have that problem)
    afx_msg void OnDestroy() { CMDIChildWnd::OnDestroy(); if (onClose) onClose(this); }
    BOOL PreTranslateMessage(MSG* p) override {
        if (p->hwnd == m_in.m_hWnd && p->message == WM_KEYDOWN && GetKeyState(VK_CONTROL) < 0 && p->wParam == 'K') {   // Ctrl+K: color picker
            CColorPickerDlg dlg(this);
            if (dlg.DoModal() == IDOK && dlg.picked >= 0) {
                CString code;
                if (dlg.pickedBg) code.Format(L",%02d", dlg.picked);           // background: appended onto whatever foreground code is already there
                else code.Format(L"%c%02d", (wchar_t)3, dlg.picked);           // foreground: a fresh color code
                m_in.ReplaceSel(code);
            }
            return TRUE;
        }
        if (p->hwnd == m_in.m_hWnd && p->message == WM_KEYDOWN && GetKeyState(VK_CONTROL) < 0) {   // Ctrl+B/U/O/I/R/E insert mIRC codes
            wchar_t c = p->wParam == 'B' ? 2 : p->wParam == 'U' ? 31 : p->wParam == 'O' ? 15
                      : p->wParam == 'I' ? 29 : p->wParam == 'R' ? 22 : p->wParam == 'E' ? 30 : 0;
            if (c) { m_in.ReplaceSel(CString(c)); return TRUE; }
        }
        if (p->hwnd == m_in.m_hWnd && p->message == WM_KEYDOWN && (p->wParam == VK_UP || p->wParam == VK_DOWN)) {   // command history
            if (!m_hist.empty()) {
                if (p->wParam == VK_UP) {
                    if (m_histPos < 0) m_histPos = (int)m_hist.size() - 1;
                    else if (m_histPos > 0) m_histPos--;
                } else {   // VK_DOWN
                    if (m_histPos >= 0) m_histPos = (m_histPos + 1 < (int)m_hist.size()) ? m_histPos + 1 : -1;
                }
                CString s = m_histPos < 0 ? CString() : m_hist[m_histPos];
                m_in.SetWindowText(s);
                int len = m_in.GetWindowTextLength(); m_in.SetSel(len, len);   // caret to end, so typing continues from there
            }
            return TRUE;
        }
        if (p->hwnd == m_in.m_hWnd && p->wParam == VK_RETURN &&
            (p->message == WM_KEYDOWN || p->message == WM_CHAR)) {
            if (p->message == WM_KEYDOWN) {
                CString s; m_in.GetWindowText(s); m_in.SetWindowText(L"");
                if (!s.IsEmpty()) {
                    if (m_hist.empty() || m_hist.back() != s) m_hist.push_back(s);   // skip exact repeats of the last entry
                    m_histPos = -1;
                    if (onInput) onInput(this, s);
                }
            }
            return TRUE;
        }
        return CMDIChildWnd::PreTranslateMessage(p);
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CChatWnd, CMDIChildWnd)
    ON_WM_CREATE() 
    ON_WM_SIZE() 
    ON_WM_SETFOCUS() 
    ON_WM_DESTROY()
    ON_WM_CTLCOLOR()
    ON_LBN_DBLCLK(4, OnNickDbl)
END_MESSAGE_MAP()

// ---------------- Status bar: click a channel name in the "Channels:" pane to open it ----------------
class CChanBar : public CStatusBar {
public:
    std::function<void(CString)> onChan;
protected:
    afx_msg void OnLButtonUp(UINT, CPoint p) {
        CRect r; GetItemRect(3, &r);
        if (!onChan || !r.PtInRect(p)) return;
        CString t; GetPaneText(3, t);
        CClientDC dc(this); if (CFont* f = GetFont()) dc.SelectObject(f);
        int n = t.GetLength();
        for (int i = 0; i < n;) {
            if (iswspace(t[i])) { i++; continue; }
            int j = i; while (j < n && !iswspace(t[j])) j++;
            int x0 = r.left + 3 + dc.GetTextExtent(t.Left(i)).cx, x1 = r.left + 3 + dc.GetTextExtent(t.Left(j)).cx;
            if (p.x >= x0 && p.x < x1) { CString w = t.Mid(i, j - i); if (w[0] == L'#' || w[0] == L'&') onChan(w); return; }
            i = j;
        }
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CChanBar, CStatusBar)
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

// ---------------- Switchbar: one button per window (Status, channels, queries) ----------------
class CSwitchBar : public CWnd {   // self-drawn buttons, positioned explicitly by the frame (no docking magic)
public:
    struct Btn { CString text; int act = 0; bool sel = false;
                 bool operator==(const Btn& o) const { return text == o.text && act == o.act && sel == o.sel; } };
    std::vector<Btn> btns;
    std::function<void(int)> onSel, onClose;
    std::function<void(int, CPoint)> onMenu;
    std::function<void(CPoint)> onBarMenu;   // right-click on empty space (not a button)
    Gdiplus::Bitmap* skin = nullptr;   // background skin image, owned by the frame; nullptr = plain color
    BOOL Create(CWnd* parent) {
        return CWnd::Create(AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1)), nullptr,
                            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, CRect(0, 0, 0, 0), parent, 1401);
    }
    void Set(const std::vector<Btn>& b) { if (!(b == btns)) { btns = b; Invalidate(); } }
    enum { HEIGHT = 28 };
protected:
    enum { BW = 112 };
    CRect BtnRect(int i, int h) const { int x = 2 + i * (BW + 2); return CRect(x, 2, x + BW, h - 2); }
    int Hit(CPoint p) {
        CRect c; GetClientRect(c);
        for (int i = 0; i < (int)btns.size(); i++) if (BtnRect(i, c.Height()).PtInRect(p)) return i;
        return -1;
    }
    afx_msg void OnPaint() {   // grey dot = idle, blue = events, red = new messages (like mIRC's window list)
        CPaintDC dc(this); CRect c; GetClientRect(c);
        if (skin) { Gdiplus::Graphics g(dc.m_hDC); g.DrawImage(skin, 0, 0, c.Width(), c.Height()); }
        else dc.FillSolidRect(c, ::GetSysColor(COLOR_BTNFACE));
        dc.SelectObject(CFont::FromHandle((HFONT)::GetStockObject(DEFAULT_GUI_FONT))); dc.SetBkMode(TRANSPARENT);
        for (int i = 0; i < (int)btns.size(); i++) {
            const Btn& b = btns[i]; CRect r = BtnRect(i, c.Height());
            if (!skin || b.sel) dc.FillSolidRect(r, ::GetSysColor(b.sel ? COLOR_WINDOW : COLOR_BTNFACE));   // skinned: let idle buttons show the image through
            dc.Draw3dRect(r, ::GetSysColor(b.sel ? COLOR_BTNSHADOW : COLOR_BTNHIGHLIGHT), ::GetSysColor(b.sel ? COLOR_BTNHIGHLIGHT : COLOR_BTNSHADOW));
            COLORREF dotc = b.act == 2 ? RGB(220, 0, 0) : b.act == 1 ? RGB(0, 0, 220) : RGB(150, 150, 150);
            CRect dot(r.left + 6, r.top + r.Height() / 2 - 4, r.left + 14, r.top + r.Height() / 2 + 4);
            CBrush br(dotc); CBrush* ob = dc.SelectObject(&br); CPen pn(PS_SOLID, 1, RGB(90, 90, 90)); CPen* op = dc.SelectObject(&pn);
            dc.Ellipse(dot); dc.SelectObject(ob); dc.SelectObject(op);
            dc.SetTextColor(b.act == 2 ? RGB(220, 0, 0) : b.act == 1 ? RGB(0, 0, 220) : ::GetSysColor(COLOR_BTNTEXT));
            CRect t = r; t.left = dot.right + 5; t.right -= 5;
            dc.DrawText(b.text, t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
    }
    afx_msg void OnLButtonDown(UINT, CPoint p) { int i = Hit(p); if (i >= 0 && onSel) onSel(i); }
    afx_msg void OnRButtonUp(UINT, CPoint p) {
        int i = Hit(p); CPoint sp = p; ClientToScreen(&sp);
        if (i >= 0) { if (onMenu) onMenu(i, sp); } else if (onBarMenu) onBarMenu(sp);
    }
    afx_msg void OnMButtonUp(UINT, CPoint p) { int i = Hit(p); if (i >= 0 && onClose) onClose(i); }   // middle-click closes
    afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }   // OnPaint always fully repaints the background itself (skin or solid)
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CSwitchBar, CWnd)
    ON_WM_PAINT() 
    ON_WM_LBUTTONDOWN() 
    ON_WM_RBUTTONUP() 
    ON_WM_MBUTTONUP()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

// ---------------- MDI client area: subclassed only to add an optional background image behind the child windows ----------------
class CMdiClient : public CWnd {
public:
    Gdiplus::Bitmap* skin = nullptr;
    std::function<void(CPoint)> onBarMenu;   // right-click anywhere on the empty workspace
protected:
    std::unique_ptr<Gdiplus::Bitmap> m_skinScaled; CSize m_skinScaledSize; Gdiplus::Bitmap* m_skinScaledSrc = nullptr;   // skin pre-stretched to the current client size
    afx_msg BOOL OnEraseBkgnd(CDC* dc) {
        if (!skin) { m_skinScaled.reset(); return Default(); }   // no image set: let the system paint its normal workspace colour
        CRect r; GetClientRect(r);
        if (r.Width() <= 0 || r.Height() <= 0) return TRUE;
        if (!m_skinScaled || m_skinScaledSize != r.Size() || m_skinScaledSrc != skin) {   // the expensive high-quality resize happens only here, not on every repaint
            auto scaled = std::make_unique<Gdiplus::Bitmap>(r.Width(), r.Height(), PixelFormat24bppRGB);
            Gdiplus::Graphics gs(scaled.get());
            gs.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            gs.DrawImage(skin, 0, 0, r.Width(), r.Height());
            m_skinScaled = std::move(scaled); m_skinScaledSize = r.Size(); m_skinScaledSrc = skin;
        }
        Gdiplus::Graphics g(dc->m_hDC); g.DrawImage(m_skinScaled.get(), 0, 0);   // a plain, fast blit: no scaling work on the hot path
        return TRUE;
    }
    afx_msg void OnRButtonUp(UINT, CPoint p) { CPoint sp = p; ClientToScreen(&sp); if (onBarMenu) onBarMenu(sp); }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CMdiClient, CWnd)
    ON_WM_ERASEBKGND()
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

// ---------------- Net: one IRC connection (its own socket, nick, options and status text) ----------------
struct Net {
    CString chanmodes = L"beI,k,l,imnpst";   // from 005 CHANMODES=: list modes, always-parameter modes, set-parameter modes, flags
    CIrcSock sock;
    bool conn = false;
    CString nick = L"User";
    Opts o;
    CString state = L"Not connected";
    CString tag;      // short label prefixed onto this network's windows in the switchbar, once there's more than one
    int id = 0;
    CListWnd* listWnd = nullptr;   // this network's open /list results window, if any (one at a time, reused on repeat /list)
    CString network;               // from the server's 005 ISUPPORT "NETWORK=" token, for $network (empty if the server doesn't say)
};

// ---------------- Channel List: /list results, sortable, right-click/double-click to join ----------------
class CListWnd : public CMDIChildWnd {
public:
    Net* net = nullptr;
    int m_seq = 0;   // creation order, same counter as CChatWnd, so the switchbar can interleave both correctly
    struct Row { CString chan; int users; CString modes; CString topic; };
    std::vector<Row> rows;
    int sortCol = 1; bool sortAsc = false;   // default: most users first, like mIRC's list window
    std::function<void(Net*, CString)> onJoin;
    std::function<void(CListWnd*)> onClosed;

    void Clear() { rows.clear(); m_list.DeleteAllItems(); }
    void AddRow(const CString& chan, int users, const CString& modes, const CString& topic) { rows.push_back({ chan, users, modes, topic }); }
    void Resort() {
        std::sort(rows.begin(), rows.end(), [this](const Row& a, const Row& b) {
            int r = (sortCol == 0) ? a.chan.CompareNoCase(b.chan) : (sortCol == 1) ? (a.users - b.users)
                  : (sortCol == 2) ? a.modes.CompareNoCase(b.modes) : a.topic.CompareNoCase(b.topic);
            return sortAsc ? (r < 0) : (r > 0);
        });
        Populate();
    }
    void Populate() {
        m_list.DeleteAllItems();
        for (size_t i = 0; i < rows.size(); i++) {
            int idx = m_list.InsertItem((int)i, rows[i].chan);
            CString u; u.Format(L"%d", rows[i].users);
            m_list.SetItemText(idx, 1, u);
            m_list.SetItemText(idx, 2, rows[i].modes);
            m_list.SetItemText(idx, 3, Strip(rows[i].topic));   // plain fallback text; OnCustomDraw renders the real colors
        }
        CString t; t.Format(L"Channel List (%d)", (int)rows.size());
        SetWindowText(t);
    }
protected:
    CListCtrl m_list;
    afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) {
        NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)pNMHDR;
        switch (cd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT: *pResult = CDRF_NOTIFYITEMDRAW; return;
        case CDDS_ITEMPREPAINT: *pResult = CDRF_NOTIFYSUBITEMDRAW; return;
        case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
            int row = (int)cd->nmcd.dwItemSpec;
            if (cd->iSubItem == 3 && row >= 0 && row < (int)rows.size()) {   // Topic column: render mIRC colors ourselves
                CDC* dc = CDC::FromHandle(cd->nmcd.hdc);
                CRect r; m_list.GetSubItemRect(row, 3, LVIR_LABEL, r);
                bool sel = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
                COLORREF bg = sel ? ::GetSysColor(COLOR_HIGHLIGHT) : ::GetSysColor(COLOR_WINDOW);
                COLORREF fg = sel ? ::GetSysColor(COLOR_HIGHLIGHTTEXT) : ::GetSysColor(COLOR_WINDOWTEXT);
                dc->FillSolidRect(r, bg); dc->SetBkMode(TRANSPARENT);
                CRect tr = r; tr.left += 4;
                DrawMircText(dc, tr, rows[row].topic, fg);
                *pResult = CDRF_SKIPDEFAULT;
                return;
            }
            *pResult = CDRF_DODEFAULT;
            return;
        }
        default: *pResult = CDRF_DODEFAULT; return;
        }
    }
    afx_msg int OnCreate(LPCREATESTRUCT cs) {
        if (CMDIChildWnd::OnCreate(cs) == -1) return -1;
        CRect z(0, 0, 0, 0);
        m_list.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER, z, this, 1);
        m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        m_list.InsertColumn(0, L"Channel", LVCFMT_LEFT, 190);
        m_list.InsertColumn(1, L"Users", LVCFMT_RIGHT, 60);
        m_list.InsertColumn(2, L"Modes", LVCFMT_LEFT, 70);
        m_list.InsertColumn(3, L"Topic", LVCFMT_LEFT, 1800);
        return 0;
    }
    afx_msg void OnSize(UINT t, int cx, int cy) { CMDIChildWnd::OnSize(t, cx, cy); if (m_list.m_hWnd) m_list.MoveWindow(0, 0, cx, cy); }
    afx_msg void OnDestroy() { CMDIChildWnd::OnDestroy(); if (onClosed) onClosed(this); }
    afx_msg void OnInitMenuPopup(CMenu*, UINT, BOOL) {
        // Deliberately skip the base class: CFrameWnd's default handling here auto-disables any menu item
        // whose command ID has no ON_COMMAND handler in the message map -- which would grey out (and make
        // unclickable) our ad-hoc "Join" popup, since its result is read directly via TPM_RETURNCMD instead.
        // This window has no real menu bar of its own, so there's nothing that legitimately needs the default.
    }
    afx_msg void OnColumnClick(NMHDR* h, LRESULT* r) {
        NMLISTVIEW* nv = (NMLISTVIEW*)h;
        if (nv->iSubItem == sortCol) sortAsc = !sortAsc; else { sortCol = nv->iSubItem; sortAsc = true; }
        Resort(); *r = 0;
    }
    afx_msg void OnDblClick(NMHDR*, LRESULT* r) {
        *r = 0;
        int i = m_list.GetNextItem(-1, LVNI_SELECTED); if (i < 0) return;
        if (onJoin) onJoin(net, m_list.GetItemText(i, 0));
    }
    afx_msg void OnRClick(NMHDR* h, LRESULT* r) {
        *r = 0;
        NMITEMACTIVATE* ia = (NMITEMACTIVATE*)h;
        int i = ia->iItem; if (i < 0) return;
        m_list.SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        CString chan = m_list.GetItemText(i, 0);
        CMenu m; m.CreatePopupMenu();
        m.AppendMenu(MF_STRING, 1, L"Join " + chan);
        CPoint pt; GetCursorPos(&pt);
        SetForegroundWindow();
        int cmd = m.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
        PostMessage(WM_NULL, 0, 0);
        if (cmd == 1 && onJoin) onJoin(net, chan);
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CListWnd, CMDIChildWnd)
    ON_WM_CREATE() ON_WM_SIZE() ON_WM_DESTROY() ON_WM_INITMENUPOPUP()
    ON_NOTIFY(LVN_COLUMNCLICK, 1, OnColumnClick) ON_NOTIFY(NM_DBLCLK, 1, OnDblClick) ON_NOTIFY(NM_RCLICK, 1, OnRClick)
    ON_NOTIFY(NM_CUSTOMDRAW, 1, OnCustomDraw)
END_MESSAGE_MAP()

// ---------------- Variables: numbers, single-operation math, $calc, wildcard matching ----------------
static bool ScanNum(const wchar_t*& p, double& v) {   // digits[.digits] only: no sign, exponent, hex, inf or nan
    const wchar_t* st = p; bool any = false;
    while (iswdigit(*p)) { p++; any = true; }
    if (*p == L'.') { p++; while (iswdigit(*p)) { p++; any = true; } }
    if (!any) return false;
    CString t(st, (int)(p - st)); v = wcstod(t, nullptr);
    return true;
}
static bool ParseNum(const CString& in, double& out) {
    CString t = in; t.Trim(); const wchar_t* p = t; bool neg = false;
    if (*p == L'-') { neg = true; p++; } else if (*p == L'+') p++;
    if (!ScanNum(p, out) || *p != 0) return false;
    if (neg) out = -out;
    return true;
}
static CString FmtNum(double v) {   // whole numbers plain, otherwise up to 5 decimals with trailing zeros dropped
    if (v != v || v > 1e300 || v < -1e300) return CString();
    CString s;
    if (v == floor(v) && fabs(v) < 1e15) s.Format(L"%.0f", v == 0 ? 0.0 : v);
    else { s.Format(L"%.5f", v); s.TrimRight(L'0'); s.TrimRight(L'.'); }
    return s;
}
// Exactly "number op number" with op one of + - * / % ^  ->  1 and the result in out; 0 = not such an expression;
// -1 = division by zero.
static int TryMath(const CString& in, CString& out) {
    std::vector<CString> t; int pos = 0;
    for (CString tok = in.Tokenize(L" ", pos); !tok.IsEmpty(); tok = in.Tokenize(L" ", pos)) t.push_back(tok);
    if (t.size() != 3 || t[1].GetLength() != 1 || !wcschr(L"+-*/%^", t[1][0])) return 0;
    double x, y; if (!ParseNum(t[0], x) || !ParseNum(t[2], y)) return 0;
    double r = 0;
    switch (t[1][0]) {
    case L'+': r = x + y; break;
    case L'-': r = x - y; break;
    case L'*': r = x * y; break;
    case L'/': if (y == 0) return -1; r = x / y; break;
    case L'%': if (y == 0) return -1; r = fmod(x, y); break;
    case L'^': r = pow(x, y); break;
    }
    out = FmtNum(r);
    return 1;
}
struct CalcParser {   // $calc(): + - * / % ^ , unary minus and parentheses, with the usual precedence
    const wchar_t* p = nullptr; bool ok = true;
    void ws() { while (*p == L' ') p++; }
    double expr() { double v = term(); for (;;) { ws(); if (*p == L'+') { p++; v += term(); } else if (*p == L'-') { p++; v -= term(); } else return v; } }
    double term() {
        double v = unary();
        for (;;) {
            ws();
            if (*p == L'*') { p++; v *= unary(); }
            else if (*p == L'/') { p++; double q = unary(); if (q == 0) ok = false; else v /= q; }
            else if (*p == L'%') { p++; double q = unary(); if (q == 0) ok = false; else v = fmod(v, q); }
            else return v;
        }
    }
    double unary() { ws(); if (*p == L'-') { p++; return -unary(); } if (*p == L'+') { p++; return unary(); } return power(); }
    double power() { double b = primary(); ws(); if (*p == L'^') { p++; double e = unary(); return pow(b, e); } return b; }
    double primary() {
        ws();
        if (*p == L'(') { p++; double v = expr(); ws(); if (*p == L')') p++; else ok = false; return v; }
        double v = 0; if (!ScanNum(p, v)) { ok = false; return 0; }
        return v;
    }
};
static bool CalcExpr(const CString& s, double& out) {
    CString t = s; CalcParser cp; cp.p = t; out = cp.expr(); cp.ws();
    return cp.ok && *cp.p == 0;
}
static bool GlobMatch(const wchar_t* pat, const wchar_t* s, bool cs = false) {   // * and ?; case-insensitive unless cs
    const wchar_t* star = nullptr; const wchar_t* ss = s;
    while (*s) {
        if (*pat == L'*') { star = pat++; ss = s; }
        else if (*pat == L'?' || (cs ? *pat == *s : towlower(*pat) == towlower(*s))) { pat++; s++; }
        else if (star) { pat = star + 1; s = ++ss; }
        else return false;
    }
    while (*pat == L'*') pat++;
    return *pat == 0;
}
struct VarEntry {
    CString name, value;            // name keeps the case it was first set with
    ULONGLONG expire = 0, nextStep = 0;   // -uN deadline / next once-a-second step (GetTickCount64 ms), 0 = none
    double step = 0; bool zeroUnset = false, noSave = false;   // -c/-z stepping; -z drops it at zero; -e keeps it out of vars.ini
};
typedef std::map<CString, VarEntry> VarMap;   // key: lowercase name including the leading '%'
struct VarScope { VarMap locals; std::vector<CString> unsetAtEnd; };   // one per script run

// ---------------- Aliases and scripts: file format, block parsing, syntax tree ----------------
struct AliasDef { CString name; std::vector<CString> lines; };   // one alias: its name and the lines of its body
struct SNode { int kind = 0; CString text; std::vector<SNode> a, b; };   // 0 command, 1 if (text = condition, a = then, b = else), 2 while, 3 label

static std::vector<CString> SplitPipes(const CString& s) {   // "a | b | c" -> commands; a '|' inside parentheses is left alone
    std::vector<CString> out; CString cur; int depth = 0;
    for (int i = 0; i < s.GetLength(); i++) {
        wchar_t c = s[i];
        if (c == L'(') depth++; else if (c == L')' && depth > 0) depth--;
        if (c == L'|' && depth == 0) { out.push_back(cur); cur.Empty(); } else cur += c;
    }
    out.push_back(cur);
    return out;
}
static int BraceDelta(const CString& s) { int d = 0; for (int i = 0; i < s.GetLength(); i++) { if (s[i] == L'{') d++; else if (s[i] == L'}') d--; } return d; }
static int MatchParen(const CString& s, int open) {   // index of the ')' matching the '(' at s[open], or -1
    int d = 0;
    for (int i = open; i < s.GetLength(); i++) { if (s[i] == L'(') d++; else if (s[i] == L')' && --d == 0) return i; }
    return -1;
}
static bool IsKw(const CString& s, const wchar_t* kw) {   // s starts with the keyword, then end / space / '(' / '{'
    int n = (int)wcslen(kw);
    if (s.GetLength() < n || s.Left(n).CompareNoCase(kw) != 0) return false;
    return s.GetLength() == n || s[n] == L' ' || s[n] == L'(' || s[n] == L'{';
}
// The lines of aliases.ini (or the alias editor) -> aliases.   "/name body"   or   "/name {" ... "}"   ;comments and /* */ are skipped.
static std::vector<AliasDef> ParseAliases(const std::vector<CString>& in) {
    std::vector<AliasDef> out; bool comment = false;
    for (size_t i = 0; i < in.size(); i++) {
        CString t = in[i];
        if (comment) { if (t.Find(L"*/") >= 0) comment = false; continue; }
        t.Trim();
        if (t.IsEmpty() || t[0] == L';') continue;
        if (t.Left(2) == L"/*") { if (t.Find(L"*/", 2) < 0) comment = true; continue; }
        if (t.Left(6).CompareNoCase(L"alias ") == 0) { t = t.Mid(6); t.TrimLeft(); if (t.Left(3).CompareNoCase(L"-l ") == 0) { t = t.Mid(3); t.TrimLeft(); } }   // remote-script style
        if (t.Left(1) == L"/") t = t.Mid(1);
        int k = 0; while (k < t.GetLength() && t[k] != L' ' && t[k] != L'{') k++;
        CString name = t.Left(k), rest = t.Mid(k); rest.Trim();
        if (name.IsEmpty()) continue;
        AliasDef ad; ad.name = name;
        int depth = BraceDelta(rest);
        if (depth <= 0) {
            if (rest.Left(1) == L"{" && rest.Right(1) == L"}") { rest = rest.Mid(1, rest.GetLength() - 2); rest.Trim(); }   // "{ cmd | cmd }" on one line
            ad.lines.push_back(rest);
        } else {   // a block: read lines until the braces balance
            bool outer = rest.Left(1) == L"{";
            if (outer) { rest = rest.Mid(1); rest.TrimLeft(); }
            if (!rest.IsEmpty()) ad.lines.push_back(rest);
            while (depth > 0 && i + 1 < in.size()) {
                CString l = in[++i]; l.Trim();
                depth += BraceDelta(l);
                if (depth <= 0) {
                    if (outer) { int cb = l.ReverseFind(L'}'); CString head = cb > 0 ? l.Left(cb) : CString(); head.Trim(); if (!head.IsEmpty()) ad.lines.push_back(head); }
                    else ad.lines.push_back(l);
                    break;
                }
                ad.lines.push_back(l);
            }
        }
        bool replaced = false;
        for (auto& e : out) if (e.name.CompareNoCase(ad.name) == 0) { e = ad; replaced = true; break; }
        if (!replaced) out.push_back(ad);
    }
    return out;
}
// Script lines -> a flat list of statements, with "{" and "}" as tokens of their own. Drops ; comments and /* */ blocks and
// joins lines that end in $&.
static std::vector<CString> ScriptTokens(const std::vector<CString>& lines) {
    std::vector<CString> joined; bool inComment = false; CString pending;
    for (size_t li = 0; li < lines.size(); li++) {
        CString t = lines[li];
        if (inComment) { int e = t.Find(L"*/"); if (e < 0) continue; t = t.Mid(e + 2); inComment = false; }
        t.Trim();
        if (t.Left(2) == L"/*") { int e = t.Find(L"*/", 2); if (e < 0) { inComment = true; continue; } t = t.Mid(e + 2); t.Trim(); }
        if (pending.IsEmpty() && (t.IsEmpty() || t[0] == L';')) continue;
        bool cont = t.GetLength() >= 2 && t.Right(2) == L"$&";
        if (cont) t = t.Left(t.GetLength() - 2);
        if (!pending.IsEmpty()) t.TrimLeft();
        pending += t;
        if (cont) continue;
        joined.push_back(pending); pending.Empty();
    }
    if (!pending.IsEmpty()) joined.push_back(pending);
    std::vector<CString> toks;
    for (size_t li = 0; li < joined.size(); li++) {
        CString t = joined[li]; t.Trim();
        while (t.Left(1) == L"}") { toks.push_back(CString(L"}")); t = t.Mid(1); t.TrimLeft(); }
        if (t.IsEmpty()) continue;
        if (t == L"{") { toks.push_back(t); continue; }
        if (t.Right(1) == L"{") { CString head = t.Left(t.GetLength() - 1); head.TrimRight(); if (!head.IsEmpty()) toks.push_back(head); toks.push_back(CString(L"{")); continue; }
        toks.push_back(t);
    }
    return toks;
}
static std::vector<SNode> ParseNodes(const std::vector<CString>& t, size_t& pos, bool inBlock);
static std::vector<SNode> ParseBody(const std::vector<CString>& t, size_t& pos, CString after) {   // what follows "if (...)": a { block } or a command on the same line
    if (after.Left(1) == L"{" && after.Right(1) == L"}") { after = after.Mid(1, after.GetLength() - 2); after.Trim(); }
    if (after.IsEmpty()) {
        if (pos < t.size() && t[pos] == L"{") { pos++; return ParseNodes(t, pos, true); }
        return std::vector<SNode>();
    }
    std::vector<CString> one; one.push_back(after); size_t p = 0;
    return ParseNodes(one, p, false);
}
static SNode ParseCtrl(const std::vector<CString>& t, size_t& pos, CString kw, CString rest, int kind) {   // if / elseif / while
    SNode n; rest.TrimLeft();
    int cl = rest.Left(1) == L"(" ? MatchParen(rest, 0) : -1;
    if (cl < 0) { n.kind = 0; n.text = kw + L" " + rest; return n; }   // malformed: leave it as a command so the user sees the error
    n.kind = kind; n.text = rest.Mid(1, cl - 1);
    CString after = rest.Mid(cl + 1); after.Trim();
    n.a = ParseBody(t, pos, after);
    if (kind == 1 && pos < t.size()) {
        CString nx = t[pos];
        if (IsKw(nx, L"elseif")) { pos++; n.b.push_back(ParseCtrl(t, pos, CString(L"elseif"), nx.Mid(6), 1)); }
        else if (IsKw(nx, L"else")) { pos++; CString ea = nx.Mid(4); ea.Trim(); n.b = ParseBody(t, pos, ea); }
    }
    return n;
}
static std::vector<SNode> ParseNodes(const std::vector<CString>& t, size_t& pos, bool inBlock) {
    std::vector<SNode> out;
    while (pos < t.size()) {
        CString s = t[pos];
        if (s == L"}") { pos++; if (inBlock) return out; continue; }
        if (s == L"{") { pos++; std::vector<SNode> inner = ParseNodes(t, pos, true); for (size_t i = 0; i < inner.size(); i++) out.push_back(inner[i]); continue; }
        pos++;
        if (IsKw(s, L"if")) { out.push_back(ParseCtrl(t, pos, CString(L"if"), s.Mid(2), 1)); continue; }
        if (IsKw(s, L"while")) { out.push_back(ParseCtrl(t, pos, CString(L"while"), s.Mid(5), 2)); continue; }
        if (s.GetLength() > 1 && s[0] == L':' && s.Find(L' ') < 0) { SNode n; n.kind = 3; n.text = s.Mid(1); out.push_back(n); continue; }   // :label
        std::vector<CString> parts = SplitPipes(s);
        for (size_t i = 0; i < parts.size(); i++) { CString c = parts[i]; c.Trim(); if (!c.IsEmpty()) { SNode n; n.text = c; out.push_back(n); } }
    }
    return out;
}

// ---------------- Alias editor: the whole alias list as text, like mIRC's alias editor ----------------
class CAliasDlg : public CDialog {
    CString& val; std::vector<WORD> t; int cnt = 0;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
public:
    CAliasDlg(CString& v, CWnd* parent, const wchar_t* title = L"Aliases", const wchar_t* hint = L"One alias per line:  /name commands     (multi-line:  /name {   lines   } )") : val(v) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(340); t.push_back(230);
        t.push_back(0); t.push_back(0); S(title); t.push_back(9); S(DEFAULT_FONT);
        Item(SS_LEFT, 6, 6, 328, 10, 0xFFFF, 0x0082, hint);
        Item(WS_BORDER | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN, 6, 20, 328, 184, 101, 0x0081, L"");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 224, 210, 50, 14, IDOK, 0x0080, L"OK");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 280, 210, 50, 14, IDCANCEL, 0x0080, L"Cancel");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetDlgItemText(101, val);
        SendDlgItemMessage(101, EM_LIMITTEXT, 0, 0);
        SendDlgItemMessage(101, EM_SETSEL, 0, 0);
        GetDlgItem(101)->SetFocus();
        return FALSE;   // focus is set by hand so the whole text isn't left selected
    }
    void OnOK() override { GetDlgItemText(101, val); CDialog::OnOK(); }
};

// ---------------- Popup menus: file format ----------------
struct PopupItem { CString title; std::vector<CString> cmd; int depth = 0; };   // cmd empty: a submenu heading, a "-" separator, or a plain label
enum { IDP_BAR = 20000, IDP_CTX = 21000, IDP_COPY = 29999 };   // menu ids: menu-bar popups / the popup being shown / the built-in Copy
static const wchar_t* const kPopSec[5] = { L"mpopup", L"cpopup", L"qpopup", L"lpopup", L"bpopup" };        // status, channel, query, nick list, menu bar
static const wchar_t* const kPopType[5] = { L"status", L"channel", L"query", L"nicklist", L"menubar" };   // what $menu returns
// "Title:/commands" one per line; leading dots make sub menus (".Sub", "..Sub sub"); "-" is a separator; "Title {" ... "}" is a multi-line item.
static std::vector<PopupItem> ParsePopupItems(const std::vector<CString>& in) {
    std::vector<PopupItem> out;
    for (size_t i = 0; i < in.size(); i++) {
        CString t = in[i]; t.TrimLeft();
        if (t.IsEmpty() || t[0] == L';') continue;
        PopupItem it; int dots = 0; while (dots < t.GetLength() && t[dots] == L'.') dots++;
        it.depth = dots; t = t.Mid(dots); t.TrimLeft();
        int c = -1, d = 0;   // the title ends at the first ':' outside parentheses (so $iif(a:b,c) is safe)
        for (int k = 0; k < t.GetLength(); k++) { if (t[k] == L'(') d++; else if (t[k] == L')' && d > 0) d--; else if (t[k] == L':' && d == 0) { c = k; break; } }
        CString cmd;
        if (c >= 0) { it.title = t.Left(c); cmd = t.Mid(c + 1); }
        else { it.title = t; if (t.Right(1) == L"{" && BraceDelta(t) > 0) { it.title = t.Left(t.GetLength() - 1); cmd = L"{"; } }
        it.title.Trim(); cmd.Trim();
        if (!cmd.IsEmpty()) {
            int db = BraceDelta(cmd);
            if (db <= 0) {
                if (cmd.Left(1) == L"{" && cmd.Right(1) == L"}") { cmd = cmd.Mid(1, cmd.GetLength() - 2); cmd.Trim(); }
                it.cmd.push_back(cmd);
            } else {   // a { ... } body over several lines
                bool outer = cmd.Left(1) == L"{";
                if (outer) { cmd = cmd.Mid(1); cmd.TrimLeft(); }
                if (!cmd.IsEmpty()) it.cmd.push_back(cmd);
                while (db > 0 && i + 1 < in.size()) {
                    CString l = in[++i]; l.Trim(); db += BraceDelta(l);
                    if (db <= 0) {
                        if (outer) { int cb = l.ReverseFind(L'}'); CString head = cb > 0 ? l.Left(cb) : CString(); head.Trim(); if (!head.IsEmpty()) it.cmd.push_back(head); }
                        else it.cmd.push_back(l);
                        break;
                    }
                    it.cmd.push_back(l);
                }
            }
        }
        if (it.title.IsEmpty() && it.cmd.empty()) continue;
        out.push_back(it);
    }
    return out;
}

// ---------------- Channel Central: /channel  (topic, modes, and the ban / except / invite / quiet lists) ----------------
enum { IDC_CC_TOPIC = 401, IDC_CC_LISTLBL, IDC_CC_LIST, IDC_CC_BANS, IDC_CC_EXC, IDC_CC_INV, IDC_CC_QUI, IDC_CC_EDIT, IDC_CC_REMOVE, IDC_CC_STATUS,
       IDC_CC_T, IDC_CC_N, IDC_CC_I, IDC_CC_M, IDC_CC_P, IDC_CC_S, IDC_CC_KEYCK, IDC_CC_KEY, IDC_CC_SHOW, IDC_CC_LIMCK, IDC_CC_LIM, IDC_CC_HELP };
class CChanCentralDlg : public CDialog {
    struct Entry { CString mask, by, when; };
    std::vector<WORD> t; int cnt = 0; CListBox m_list;
    std::vector<Entry> lists[4]; bool loaded[4] = { false, false, false, false }, requested[4] = { false, false, false, false };
    int cur = 0; bool origKnown = false; CString origFlags, origKey; int origLimit = 0; bool hasE = true, hasI = true, hasQ = false;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
    static std::vector<CString> Groups(const CString& s) {   // "beI,k,l,imnpst" -> 4 groups (empty groups kept)
        std::vector<CString> g; int st = 0;
        for (;;) { int c = s.Find(L',', st); if (c < 0) { g.push_back(s.Mid(st)); break; } g.push_back(s.Mid(st, c - st)); st = c + 1; }
        return g;
    }
    static wchar_t Letter(int ty) { return L"beIq"[ty]; }   // the mode letter behind each list: bans, excepts, invites, quiets
    static CString FmtWhen(const CString& w) {
        __int64 v = _wtoi64(w); if (v <= 0) return w;
        return CTime((__time64_t)v).Format(L"%Y-%m-%d %H:%M");
    }
    static CString Disp(const Entry& e) {
        CString s = e.mask;
        if (!e.by.IsEmpty()) s += L"   set by " + e.by;
        if (!e.when.IsEmpty()) s += L"   " + FmtWhen(e.when);
        return s;
    }
    void FixExtent() {   // lets the list scroll sideways to fit its longest line
        CDC* dc = m_list.GetDC(); if (!dc) return;
        CFont* f = m_list.GetFont(); CFont* old = f ? dc->SelectObject(f) : nullptr;
        int w = 0;
        for (int i = 0; i < m_list.GetCount(); i++) { CString s; m_list.GetText(i, s); w = (std::max)(w, (int)dc->GetTextExtent(s).cx); }
        if (old) dc->SelectObject(old);
        m_list.ReleaseDC(dc); m_list.SetHorizontalExtent(w + 8);
    }
    void UpdateButtons() { BOOL sel = m_list.GetCurSel() != LB_ERR; GetDlgItem(IDC_CC_EDIT)->EnableWindow(sel); GetDlgItem(IDC_CC_REMOVE)->EnableWindow(sel); }
    void SetStatus(const CString& s) { if (m_hWnd) SetDlgItemText(IDC_CC_STATUS, s); }
    void RefreshList() {
        if (!m_hWnd) return;
        m_list.ResetContent();
        for (size_t i = 0; i < lists[cur].size(); i++) m_list.AddString(Disp(lists[cur][i]));
        FixExtent(); UpdateButtons();
    }
    CString CountText(int ty) { CString s; s.Format(L"%d entr%s", (int)lists[ty].size(), lists[ty].size() == 1 ? L"y" : L"ies"); return s; }
    void SwitchList(int ty) {
        cur = ty; CheckRadioButton(IDC_CC_BANS, IDC_CC_QUI, IDC_CC_BANS + ty);
        static const wchar_t* nm[4] = { L"Bans", L"Excepts", L"Invites", L"Quiets" };
        SetDlgItemText(IDC_CC_LISTLBL, CString(nm[ty]) + L" List:");
        if (!requested[ty] && sendRaw) { requested[ty] = true; sendRaw(L"MODE " + chan + L" +" + CString(Letter(ty))); }   // the lists are fetched when first opened
        SetStatus(loaded[ty] ? CountText(ty) : CString(L"Loading the list..."));
        RefreshList();
    }
    void ApplyModesToControls() {
        if (!m_hWnd || !origKnown) return;
        static const struct { UINT id; wchar_t c; } fl[] = { { IDC_CC_T, L't' }, { IDC_CC_N, L'n' }, { IDC_CC_I, L'i' }, { IDC_CC_M, L'm' }, { IDC_CC_P, L'p' }, { IDC_CC_S, L's' } };
        for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) CheckDlgButton(fl[i].id, origFlags.Find(fl[i].c) >= 0);
        bool hk = origFlags.Find(L'k') >= 0, hl = origFlags.Find(L'l') >= 0;
        CheckDlgButton(IDC_CC_KEYCK, hk); SetDlgItemText(IDC_CC_KEY, origKey == L"*" ? CString() : origKey); GetDlgItem(IDC_CC_KEY)->EnableWindow(hk);   // a key the server hides shows as "*"
        CheckDlgButton(IDC_CC_LIMCK, hl); if (hl) SetDlgItemInt(IDC_CC_LIM, origLimit, FALSE); GetDlgItem(IDC_CC_LIM)->EnableWindow(hl);
    }
public:
    Net* net = nullptr; CString chan, chanModes = L"beI,k,l,imnpst", curTopic; std::vector<CString> topicHist;
    std::function<void(const CString&)> sendRaw;   // sends a raw line to this channel's server

    CChanCentralDlg(const CString& c, CWnd* parent) : chan(c) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(326); t.push_back(266);
        t.push_back(0); t.push_back(0); S(L"Channel Central " + chan); t.push_back(9); S(DEFAULT_FONT);
        Item(SS_LEFT, 8, 6, 200, 9, 0xFFFF, 0x0082, L"Topic history:");
        Item(CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, 8, 16, 310, 90, IDC_CC_TOPIC, 0x0085, L"");
        Item(SS_LEFT, 8, 36, 200, 9, IDC_CC_LISTLBL, 0x0082, L"Bans List:");
        Item(LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_HSCROLL | WS_BORDER | WS_TABSTOP, 8, 46, 310, 70, IDC_CC_LIST, 0x0083, L"");
        Item(BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_GROUP | WS_TABSTOP, 62, 122, 56, 14, IDC_CC_BANS, 0x0080, L"Bans");
        Item(BS_AUTORADIOBUTTON | BS_PUSHLIKE, 122, 122, 56, 14, IDC_CC_EXC, 0x0080, L"Excepts");
        Item(BS_AUTORADIOBUTTON | BS_PUSHLIKE, 182, 122, 56, 14, IDC_CC_INV, 0x0080, L"Invites");
        Item(BS_AUTORADIOBUTTON | BS_PUSHLIKE, 62, 140, 56, 14, IDC_CC_QUI, 0x0080, L"Quiets");
        Item(BS_PUSHBUTTON | WS_GROUP | WS_TABSTOP, 122, 140, 56, 14, IDC_CC_EDIT, 0x0080, L"Edit...");
        Item(BS_PUSHBUTTON, 182, 140, 56, 14, IDC_CC_REMOVE, 0x0080, L"Remove");
        Item(SS_LEFT, 8, 158, 310, 9, IDC_CC_STATUS, 0x0082, L"");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 14, 170, 140, 10, IDC_CC_T, 0x0080, L"Operators set topic");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 14, 182, 140, 10, IDC_CC_N, 0x0080, L"No external messages");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 14, 194, 140, 10, IDC_CC_I, 0x0080, L"Invite only channel");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 14, 206, 140, 10, IDC_CC_M, 0x0080, L"Moderated channel");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 14, 218, 140, 10, IDC_CC_P, 0x0080, L"Private channel");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 14, 230, 140, 10, IDC_CC_S, 0x0080, L"Secret channel");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 172, 170, 100, 10, IDC_CC_KEYCK, 0x0080, L"Channel key:");
        Item(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, 172, 182, 100, 12, IDC_CC_KEY, 0x0081, L"");
        Item(BS_AUTOCHECKBOX | BS_PUSHLIKE | WS_TABSTOP, 276, 182, 38, 12, IDC_CC_SHOW, 0x0080, L"Show");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 172, 202, 100, 10, IDC_CC_LIMCK, 0x0080, L"Maximum users:");
        Item(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, 172, 214, 46, 12, IDC_CC_LIM, 0x0081, L"");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 76, 246, 52, 14, IDOK, 0x0080, L"OK");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 134, 246, 52, 14, IDCANCEL, 0x0080, L"Cancel");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 192, 246, 52, 14, IDC_CC_HELP, 0x0080, L"Help");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    // ---- called as the server's replies arrive ----
    void SetModes(const CString& modes, const std::vector<CString>& args) {   // 324: e.g. "+ntkl" with args "secret" "50"
        std::vector<CString> grp = Groups(chanModes);
        origKnown = true; origFlags.Empty(); origKey.Empty(); origLimit = 0;
        size_t ai = 0; bool plus = true;
        for (int i = 0; i < modes.GetLength(); i++) {
            wchar_t c = modes[i];
            if (c == L'+') { plus = true; continue; }
            if (c == L'-') { plus = false; continue; }
            if (!plus) continue;
            origFlags += c;
            bool takesArg = (grp.size() > 1 && grp[1].Find(c) >= 0) || (grp.size() > 2 && grp[2].Find(c) >= 0);   // these come with a parameter in the reply
            if (takesArg) { CString a = ai < args.size() ? args[ai++] : CString(); if (c == L'k') origKey = a; else if (c == L'l') origLimit = _wtoi(a); }
        }
        ApplyModesToControls();
        if (!loaded[cur]) SetStatus(L"Loading the list...");
    }
    void AddEntry(int ty, const CString& mask, const CString& by, const CString& when) {
        Entry e; e.mask = mask; e.by = by; e.when = when; lists[ty].push_back(e);
        if (m_hWnd && ty == cur) { m_list.AddString(Disp(e)); FixExtent(); }
    }
    void EndList(int ty) { loaded[ty] = true; if (m_hWnd && ty == cur) SetStatus(CountText(ty)); }
    void Status(const CString& s) { SetStatus(s); }

    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        m_list.SubclassDlgItem(IDC_CC_LIST, this);
        for (size_t i = 0; i < topicHist.size(); i++) SendDlgItemMessage(IDC_CC_TOPIC, CB_ADDSTRING, 0, (LPARAM)(LPCWSTR)topicHist[i]);
        SetDlgItemText(IDC_CC_TOPIC, curTopic);
        std::vector<CString> grp = Groups(chanModes);
        if (!grp.empty()) { hasE = grp[0].Find(L'e') >= 0; hasI = grp[0].Find(L'I') >= 0; hasQ = grp[0].Find(L'q') >= 0; }   // which lists this network has
        GetDlgItem(IDC_CC_EXC)->EnableWindow(hasE); GetDlgItem(IDC_CC_INV)->EnableWindow(hasI); GetDlgItem(IDC_CC_QUI)->EnableWindow(hasQ);
        GetDlgItem(IDC_CC_KEY)->EnableWindow(FALSE); GetDlgItem(IDC_CC_LIM)->EnableWindow(FALSE);
        SendDlgItemMessage(IDC_CC_KEY, EM_SETPASSWORDCHAR, L'*', 0);
        CheckRadioButton(IDC_CC_BANS, IDC_CC_QUI, IDC_CC_BANS);
        requested[0] = true;
        if (sendRaw) { sendRaw(L"MODE " + chan); sendRaw(L"MODE " + chan + L" +b"); }   // ask for the modes and the ban list
        SetStatus(L"Asking the server for the modes and the ban list...");
        ApplyModesToControls(); RefreshList();
        return TRUE;
    }
    void OnOK() override {   // only what changed is sent
        CString tp; GetDlgItemText(IDC_CC_TOPIC, tp);
        if (sendRaw && tp != curTopic) sendRaw(L"TOPIC " + chan + L" :" + tp);
        if (sendRaw && origKnown) {
            static const struct { UINT id; wchar_t c; } fl[] = { { IDC_CC_T, L't' }, { IDC_CC_N, L'n' }, { IDC_CC_I, L'i' }, { IDC_CC_M, L'm' }, { IDC_CC_P, L'p' }, { IDC_CC_S, L's' } };
            CString plus, minus;
            for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) {
                bool was = origFlags.Find(fl[i].c) >= 0, now = IsDlgButtonChecked(fl[i].id) != 0;
                if (now && !was) plus += fl[i].c; else if (!now && was) minus += fl[i].c;
            }
            CString line; if (!plus.IsEmpty()) line += L"+" + plus; if (!minus.IsEmpty()) line += L"-" + minus;
            if (!line.IsEmpty()) sendRaw(L"MODE " + chan + L" " + line);
            CString key; GetDlgItemText(IDC_CC_KEY, key); key.Trim();
            bool hadK = origFlags.Find(L'k') >= 0, keepHidden = hadK && origKey == L"*" && key.IsEmpty();
            bool wantK = IsDlgButtonChecked(IDC_CC_KEYCK) != 0 && (!key.IsEmpty() || keepHidden);
            if (hadK && !wantK) sendRaw(L"MODE " + chan + L" -k " + (origKey.IsEmpty() ? CString(L"*") : origKey));
            else if (wantK && !keepHidden && (!hadK || key != origKey)) { if (hadK) sendRaw(L"MODE " + chan + L" -k " + origKey); sendRaw(L"MODE " + chan + L" +k " + key); }
            int lim = (int)GetDlgItemInt(IDC_CC_LIM, nullptr, FALSE);
            bool hadL = origFlags.Find(L'l') >= 0, wantL = IsDlgButtonChecked(IDC_CC_LIMCK) != 0 && lim > 0;
            if (hadL && !wantL) sendRaw(L"MODE " + chan + L" -l");
            else if (wantL && (!hadL || lim != origLimit)) { CString ls; ls.Format(L"%d", lim); sendRaw(L"MODE " + chan + L" +l " + ls); }
        }
        CDialog::OnOK();
    }
    afx_msg void OnBans() { SwitchList(0); }
    afx_msg void OnExc() { SwitchList(1); }
    afx_msg void OnInv() { SwitchList(2); }
    afx_msg void OnQui() { SwitchList(3); }
    afx_msg void OnSel() { UpdateButtons(); }
    afx_msg void OnRemoveBtn() {   // applied at once, as in mIRC
        int i = m_list.GetCurSel(); if (i == LB_ERR || i >= (int)lists[cur].size()) return;
        if (sendRaw) sendRaw(L"MODE " + chan + L" -" + CString(Letter(cur)) + L" " + lists[cur][i].mask);
        lists[cur].erase(lists[cur].begin() + i); RefreshList(); SetStatus(CountText(cur));
    }
    afx_msg void OnEditBtn() {   // replaces the selected mask with an edited one
        int i = m_list.GetCurSel(); if (i == LB_ERR || i >= (int)lists[cur].size()) return;
        CString old = lists[cur][i].mask, nm = old;
        CPromptDlg dlg(nm, L"Edit", L"Mask:", this);
        if (dlg.DoModal() != IDOK) return;
        nm.Trim(); if (nm.IsEmpty() || nm == old) return;
        CString l = CString(Letter(cur));
        if (sendRaw) sendRaw(L"MODE " + chan + L" -" + l + L"+" + l + L" " + old + L" " + nm);
        lists[cur][i].mask = nm; RefreshList();
    }
    afx_msg void OnKeyCk() { GetDlgItem(IDC_CC_KEY)->EnableWindow(IsDlgButtonChecked(IDC_CC_KEYCK) != 0); }
    afx_msg void OnLimCk() { GetDlgItem(IDC_CC_LIM)->EnableWindow(IsDlgButtonChecked(IDC_CC_LIMCK) != 0); }
    afx_msg void OnShow() {   // show / hide the key as you type it
        SendDlgItemMessage(IDC_CC_KEY, EM_SETPASSWORDCHAR, IsDlgButtonChecked(IDC_CC_SHOW) ? 0 : L'*', 0);
        GetDlgItem(IDC_CC_KEY)->Invalidate();
    }
    afx_msg void OnHelp() {
        AfxMessageBox(L"Tick the modes you want and press OK; only what you changed is sent.\n\nThe topic is set from the box at the top, and earlier topics are in its drop-down.\n\n"
                      L"Bans, Excepts, Invites and Quiets switch the list below (a list is fetched the first time you open it; you need to be a channel operator "
                      L"to see some of them). Select an entry, then Edit... or Remove: those take effect immediately.", MB_ICONINFORMATION);
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CChanCentralDlg, CDialog)
    ON_BN_CLICKED(IDC_CC_BANS, OnBans) ON_BN_CLICKED(IDC_CC_EXC, OnExc) ON_BN_CLICKED(IDC_CC_INV, OnInv) ON_BN_CLICKED(IDC_CC_QUI, OnQui)
    ON_BN_CLICKED(IDC_CC_EDIT, OnEditBtn) ON_BN_CLICKED(IDC_CC_REMOVE, OnRemoveBtn) ON_BN_CLICKED(IDC_CC_KEYCK, OnKeyCk)
    ON_BN_CLICKED(IDC_CC_LIMCK, OnLimCk) ON_BN_CLICKED(IDC_CC_SHOW, OnShow) ON_BN_CLICKED(IDC_CC_HELP, OnHelp)
    ON_LBN_SELCHANGE(IDC_CC_LIST, OnSel) ON_LBN_DBLCLK(IDC_CC_LIST, OnEditBtn)
END_MESSAGE_MAP()

// ---------------- Colors dialog: named color schemes (File > Colors...) ----------------
enum { IDC_CD_SCHEME = 501, IDC_CD_NEW, IDC_CD_DELETE, IDC_CD_LIST, IDC_CD_CHOOSE,
       IDC_CD_BGLBL, IDC_CD_BG, IDC_CD_EDITLBL, IDC_CD_EDIT, IDC_CD_NICKLBL, IDC_CD_NICK, IDC_CD_DEFAULT, IDC_CD_HELP };
class CColorsDlg : public CDialog {
    std::vector<WORD> t; int cnt = 0;
    CListBox m_list; CColorSwatch m_bgSwatch, m_editSwatch, m_nickSwatch;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
    static const wchar_t* const kNames[19];
    static COLORREF ColorScheme::* const kSlot[19];
    std::vector<ColorScheme>& out;   // CMainFrame's actual list; only overwritten (from work) if OK is pressed
    std::vector<ColorScheme> work;   // an editable copy, so Cancel leaves the original untouched
    int cur;
    void RefillSchemeCombo() {
        SendDlgItemMessage(IDC_CD_SCHEME, CB_RESETCONTENT, 0, 0);
        for (auto& s : work) SendDlgItemMessage(IDC_CD_SCHEME, CB_ADDSTRING, 0, (LPARAM)(LPCWSTR)s.name);
        SendDlgItemMessage(IDC_CD_SCHEME, CB_SETCURSEL, cur, 0);
    }
    void UpdateSwatches() {
        ColorScheme& s = work[cur];
        m_bgSwatch.color = s.chatBg == CLR_NONE ? ::GetSysColor(COLOR_WINDOW) : s.chatBg; if (m_bgSwatch.m_hWnd) m_bgSwatch.Invalidate();
        m_editSwatch.color = s.editBg == CLR_NONE ? ::GetSysColor(COLOR_WINDOW) : s.editBg; if (m_editSwatch.m_hWnd) m_editSwatch.Invalidate();
        m_nickSwatch.color = s.nickBg == CLR_NONE ? ::GetSysColor(COLOR_WINDOW) : s.nickBg; if (m_nickSwatch.m_hWnd) m_nickSwatch.Invalidate();
    }
    void SelectScheme(int i) {
        cur = i; SendDlgItemMessage(IDC_CD_SCHEME, CB_SETCURSEL, cur, 0);
        m_list.SetCurSel(0); UpdateSwatches();
    }
    void Pick(COLORREF ColorScheme::* slot) {
        COLORREF cur0 = work[cur].*slot; if (cur0 == CLR_NONE) cur0 = ::GetSysColor(COLOR_WINDOW);
        CColorDialog dlg(cur0, CC_FULLOPEN, this);
        if (dlg.DoModal() == IDOK) { work[cur].*slot = dlg.GetColor(); UpdateSwatches(); }
    }
public:
    CColorsDlg(std::vector<ColorScheme>& allSchemes, int active, CWnd* parent) : out(allSchemes), work(allSchemes), cur(active) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(250); t.push_back(275);
        t.push_back(0); t.push_back(0); S(L"Colors"); t.push_back(9); S(DEFAULT_FONT);
        Item(SS_LEFT, 8, 8, 60, 10, 0xFFFF, 0x0082, L"Scheme:");
        Item(CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP, 8, 18, 140, 90, IDC_CD_SCHEME, 0x0085, L"");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 152, 17, 44, 14, IDC_CD_NEW, 0x0080, L"New...");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 198, 17, 44, 14, IDC_CD_DELETE, 0x0080, L"Delete");
        Item(SS_LEFT, 8, 38, 100, 9, 0xFFFF, 0x0082, L"Text colors:");
        Item(LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_BORDER | WS_TABSTOP, 8, 48, 150, 160, IDC_CD_LIST, 0x0083, L"");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 166, 48, 76, 14, IDC_CD_CHOOSE, 0x0080, L"Choose...");
        Item(SS_LEFT, 166, 72, 76, 9, IDC_CD_BGLBL, 0x0082, L"Background:");
        Item(SS_NOTIFY | WS_TABSTOP, 166, 82, 76, 18, IDC_CD_BG, 0x0082, L"");
        Item(SS_LEFT, 166, 106, 76, 9, IDC_CD_EDITLBL, 0x0082, L"Editbox:");
        Item(SS_NOTIFY | WS_TABSTOP, 166, 116, 76, 18, IDC_CD_EDIT, 0x0082, L"");
        Item(SS_LEFT, 166, 140, 76, 9, IDC_CD_NICKLBL, 0x0082, L"Nicklist:");
        Item(SS_NOTIFY | WS_TABSTOP, 166, 150, 76, 18, IDC_CD_NICK, 0x0082, L"");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 8, 215, 60, 14, IDC_CD_DEFAULT, 0x0080, L"Default");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 40, 250, 50, 14, IDOK, 0x0080, L"OK");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 98, 250, 50, 14, IDCANCEL, 0x0080, L"Cancel");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 156, 250, 50, 14, IDC_CD_HELP, 0x0080, L"Help");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        m_list.SubclassDlgItem(IDC_CD_LIST, this);
        m_bgSwatch.SubclassDlgItem(IDC_CD_BG, this); m_bgSwatch.onClick = [this] { Pick(&ColorScheme::chatBg); };
        m_bgSwatch.onClear = [this] { work[cur].chatBg = CLR_NONE; UpdateSwatches(); };
        m_editSwatch.SubclassDlgItem(IDC_CD_EDIT, this); m_editSwatch.onClick = [this] { Pick(&ColorScheme::editBg); };
        m_editSwatch.onClear = [this] { work[cur].editBg = CLR_NONE; UpdateSwatches(); };
        m_nickSwatch.SubclassDlgItem(IDC_CD_NICK, this); m_nickSwatch.onClick = [this] { Pick(&ColorScheme::nickBg); };
        m_nickSwatch.onClear = [this] { work[cur].nickBg = CLR_NONE; UpdateSwatches(); };
        for (int i = 0; i < 19; i++) m_list.AddString(kNames[i]);
        RefillSchemeCombo(); SelectScheme(cur);
        return TRUE;
    }
    afx_msg void OnSchemeChange() { int i = (int)SendDlgItemMessage(IDC_CD_SCHEME, CB_GETCURSEL, 0, 0); if (i >= 0) SelectScheme(i); }
    afx_msg void OnNew() {
        CString nm; CPromptDlg dlg(nm, L"New Scheme", L"Scheme name:", this);
        if (dlg.DoModal() != IDOK) return;
        nm.Trim(); nm.Remove(L',');   // the ini line is comma-separated; a comma in the name would corrupt it on reload
        if (nm.IsEmpty()) return;
        ColorScheme ns = work[cur]; ns.name = nm; work.push_back(ns);
        RefillSchemeCombo(); SelectScheme((int)work.size() - 1);
    }
    afx_msg void OnDelete() {
        if (work.size() <= 1) { AfxMessageBox(L"At least one scheme must remain."); return; }
        work.erase(work.begin() + cur); if (cur >= (int)work.size()) cur = (int)work.size() - 1;
        RefillSchemeCombo(); SelectScheme(cur);
    }
    afx_msg void OnChoose() {
        int i = m_list.GetCurSel(); if (i < 0) return;
        COLORREF v = work[cur].*kSlot[i];
        CColorDialog dlg(v, CC_FULLOPEN, this);
        if (dlg.DoModal() == IDOK) work[cur].*kSlot[i] = dlg.GetColor();
    }
    afx_msg void OnDefaultBtn() {   // resets the current scheme's colors to the built-in defaults, keeping its name
        CString nm = work[cur].name; ColorScheme d; d.name = nm; work[cur] = d; UpdateSwatches();
    }
    afx_msg void OnHelpBtn() {
        AfxMessageBox(L"Pick a scheme, or use New... to start one from the current colors.\n\n"
                      L"The list on the left is the color used for each kind of message text; select one and click Choose... "
                      L"to change it. Background, Editbox and Nicklist are separate: click one of those boxes directly to "
                      L"change that area's background color; right-click one to reset it back to the default.\n\n"
                      L"Default resets the selected scheme's colors (not its name). Changes only take effect once you click OK.", MB_ICONINFORMATION);
    }
    void OnOK() override { out = work; CDialog::OnOK(); }
    int Active() const { return cur; }
    DECLARE_MESSAGE_MAP()
};

const wchar_t* const CColorsDlg::kNames[19] = { 
    L"Normal",
    L"CTCP",
    L"Highlight",
    L"Invite",
    L"Join",
    L"Part",
    L"Quit",
    L"Mode",
    L"Topic",
    L"Kick",
    L"Nickname",
    L"Own",
    L"Notice",
    L"Action",
    L"Other",
    L"Info",
    L"Info2",
    L"Wallops",
    L"Whois"
};

COLORREF ColorScheme::* const CColorsDlg::kSlot[19] = { 
    &ColorScheme::normal,
    &ColorScheme::ctcp,
    &ColorScheme::highlight,
    &ColorScheme::invite,
    &ColorScheme::join,
    &ColorScheme::part,
    &ColorScheme::quit,
    &ColorScheme::mode,
    &ColorScheme::topic,
    &ColorScheme::kick,
    &ColorScheme::nickname,
    &ColorScheme::own,
    &ColorScheme::notice,  
    &ColorScheme::action,
    &ColorScheme::other,
    &ColorScheme::info,
    &ColorScheme::info2,
    &ColorScheme::wallops,
    &ColorScheme::whois
};

BEGIN_MESSAGE_MAP(CColorsDlg, CDialog)
    ON_CBN_SELCHANGE(IDC_CD_SCHEME, OnSchemeChange)
    ON_BN_CLICKED(IDC_CD_NEW, OnNew) ON_BN_CLICKED(IDC_CD_DELETE, OnDelete) ON_BN_CLICKED(IDC_CD_CHOOSE, OnChoose)
    ON_BN_CLICKED(IDC_CD_DEFAULT, OnDefaultBtn) ON_BN_CLICKED(IDC_CD_HELP, OnHelpBtn)
END_MESSAGE_MAP()

// ---------------- Logging dialog: chat history on/off and where it's saved (File > Logging...) ----------------
enum { IDC_LG_ENABLE = 601, IDC_LG_FOLDER, IDC_LG_BROWSE, IDC_LG_HELP };
class CLoggingDlg : public CDialog {
    std::vector<WORD> t; int cnt = 0;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
public:
    bool enabled; CString folder;   // in/out: current settings in, the (possibly edited) settings out if OK is pressed
    CLoggingDlg(bool en, const CString& fld, CWnd* parent) : enabled(en), folder(fld) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(270); t.push_back(112);
        t.push_back(0); t.push_back(0); S(L"Logging"); t.push_back(9); S(DEFAULT_FONT);
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 8, 220, 10, IDC_LG_ENABLE, 0x0080, L"Log chat history to disk");
        Item(SS_LEFT, 8, 24, 60, 9, 0xFFFF, 0x0082, L"Log folder:");
        Item(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 8, 34, 190, 12, IDC_LG_FOLDER, 0x0081, L"");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 202, 34, 60, 12, IDC_LG_BROWSE, 0x0080, L"Browse...");
        Item(SS_LEFT, 8, 52, 254, 28, 0xFFFF, 0x0082,
             L"One file per network and window (e.g. Libera_#channel.log), plain text, appended to on every line.");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 68, 88, 50, 14, IDOK, 0x0080, L"OK");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 124, 88, 50, 14, IDCANCEL, 0x0080, L"Cancel");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 180, 88, 50, 14, IDC_LG_HELP, 0x0080, L"Help");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        CheckDlgButton(IDC_LG_ENABLE, enabled);
        SetDlgItemText(IDC_LG_FOLDER, folder);
        return TRUE;
    }
    afx_msg void OnBrowse() {
        BROWSEINFOW bi = {}; wchar_t buf[MAX_PATH] = {};
        CString cur; GetDlgItemText(IDC_LG_FOLDER, cur);
        bi.hwndOwner = m_hWnd; bi.pszDisplayName = buf; bi.lpszTitle = L"Choose a folder for chat logs"; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl) { if (SHGetPathFromIDListW(pidl, buf)) SetDlgItemText(IDC_LG_FOLDER, buf); CoTaskMemFree(pidl); }
    }
    afx_msg void OnHelpBtn() {
        AfxMessageBox(L"When enabled, every line shown in a status, channel or query window is appended to a plain-text "
                      L"file in the folder below, one file per network and window. Color codes are stripped; timestamps "
                      L"are kept. The folder is created automatically if it doesn't exist yet.", MB_ICONINFORMATION);
    }
    void OnOK() override {
        enabled = IsDlgButtonChecked(IDC_LG_ENABLE) != 0;
        GetDlgItemText(IDC_LG_FOLDER, folder); folder.Trim();
        CDialog::OnOK();
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CLoggingDlg, CDialog)
    ON_BN_CLICKED(IDC_LG_BROWSE, OnBrowse) ON_BN_CLICKED(IDC_LG_HELP, OnHelpBtn)
END_MESSAGE_MAP()

// ---------------- Identd server settings dialog (File > Identd Server...) ----------------
enum { IDC_ID_ENABLE = 661, IDC_ID_USERID, IDC_ID_SYSTEM, IDC_ID_PORT, IDC_ID_SHOWREQ, IDC_ID_ONLYCONN, IDC_ID_USEEMAIL, IDC_ID_HELP };
class CIdentdDlg : public CDialog {
    std::vector<WORD> t; int cnt = 0;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
public:
    bool enabled, showReq, onlyConnecting, useEmail; CString userId, system; int port;
    CIdentdDlg(bool en, const CString& uid, const CString& sys, int prt, bool show, bool onlyConn, bool useE, CWnd* parent)
        : enabled(en), showReq(show), onlyConnecting(onlyConn), useEmail(useE), userId(uid), system(sys), port(prt) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(240); t.push_back(150);
        t.push_back(0); t.push_back(0); S(L"Identd Server"); t.push_back(9); S(DEFAULT_FONT);
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 8, 200, 10, IDC_ID_ENABLE, 0x0080, L"Enable Identd Server");
        Item(SS_LEFT, 8, 24, 60, 9, 0xFFFF, 0x0082, L"User ID:");
        Item(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 70, 22, 100, 12, IDC_ID_USERID, 0x0081, L"");
        Item(SS_LEFT, 8, 40, 60, 9, 0xFFFF, 0x0082, L"System:");
        Item(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 70, 38, 100, 12, IDC_ID_SYSTEM, 0x0081, L"");
        Item(SS_LEFT, 8, 56, 60, 9, 0xFFFF, 0x0082, L"Port:");
        Item(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, 70, 54, 50, 12, IDC_ID_PORT, 0x0081, L"");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 72, 220, 10, IDC_ID_SHOWREQ, 0x0080, L"Show Identd requests");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 86, 220, 10, IDC_ID_ONLYCONN, 0x0080, L"Enable only when connecting");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 100, 220, 10, IDC_ID_USEEMAIL, 0x0080, L"Use ID from email address");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 34, 122, 50, 14, IDOK, 0x0080, L"OK");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 90, 122, 50, 14, IDCANCEL, 0x0080, L"Cancel");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 146, 122, 50, 14, IDC_ID_HELP, 0x0080, L"Help");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        CheckDlgButton(IDC_ID_ENABLE, enabled); CheckDlgButton(IDC_ID_SHOWREQ, showReq);
        CheckDlgButton(IDC_ID_ONLYCONN, onlyConnecting); CheckDlgButton(IDC_ID_USEEMAIL, useEmail);
        SetDlgItemText(IDC_ID_USERID, userId); SetDlgItemText(IDC_ID_SYSTEM, system);
        SetDlgItemInt(IDC_ID_PORT, port, FALSE);
        GetDlgItem(IDC_ID_USERID)->EnableWindow(!useEmail);
        return TRUE;
    }
    afx_msg void OnUseEmail() { GetDlgItem(IDC_ID_USERID)->EnableWindow(!IsDlgButtonChecked(IDC_ID_USEEMAIL)); }
    afx_msg void OnHelpBtn() {
        AfxMessageBox(L"When enabled, this answers \"ident\" queries on the given port (normally 113) with the User ID "
                      L"and System you set below -- some IRC servers refuse a connection without a reply to one.\n\n"
                      L"\"Use ID from email address\" isn't quite literal here, since this client doesn't have a separate "
                      L"email field: it uses your connection's Username (ident) field instead.\n\n"
                      L"\"Enable only when connecting\" starts the server right as you connect and stops it again the "
                      L"moment a request is answered (or shortly after, if none arrives).", MB_ICONINFORMATION);
    }
    void OnOK() override {
        enabled = IsDlgButtonChecked(IDC_ID_ENABLE) != 0; showReq = IsDlgButtonChecked(IDC_ID_SHOWREQ) != 0;
        onlyConnecting = IsDlgButtonChecked(IDC_ID_ONLYCONN) != 0; useEmail = IsDlgButtonChecked(IDC_ID_USEEMAIL) != 0;
        GetDlgItemText(IDC_ID_USERID, userId); GetDlgItemText(IDC_ID_SYSTEM, system);
        port = (int)GetDlgItemInt(IDC_ID_PORT, nullptr, FALSE);
        userId.Trim(); system.Trim(); if (system.IsEmpty()) system = L"UNIX"; if (port <= 0) port = 113;
        CDialog::OnOK();
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CIdentdDlg, CDialog)
    ON_BN_CLICKED(IDC_ID_USEEMAIL, OnUseEmail) ON_BN_CLICKED(IDC_ID_HELP, OnHelpBtn)
END_MESSAGE_MAP()

// A small square that just draws whatever HICON it's given, centered -- used by the Tray dialog's icon preview.
class CIconPreview : public CStatic {
public:
    HICON icon = nullptr;
protected:
    afx_msg void OnPaint() {
        CPaintDC dc(this); CRect r; GetClientRect(r);
        dc.FillSolidRect(r, ::GetSysColor(COLOR_WINDOW));
        dc.Draw3dRect(r, ::GetSysColor(COLOR_BTNSHADOW), ::GetSysColor(COLOR_BTNHIGHLIGHT));
        if (icon) dc.DrawIcon(r.left + (r.Width() - 32) / 2, r.top + (r.Height() - 32) / 2, icon);
    }
    afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CIconPreview, CStatic)
    ON_WM_PAINT() ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

// ---------------- Tray settings dialog (File > Tray...) ----------------
enum { IDC_TR_ALWAYS = 681, IDC_TR_STARTMIN, IDC_TR_ONMIN, IDC_TR_ANIMATE, IDC_TR_SINGLECLICK, IDC_TR_PREVIEW, IDC_TR_DEFAULT, IDC_TR_SELECT };
class CTrayDlg : public CDialog {
    std::vector<WORD> t; int cnt = 0; CIconPreview m_preview;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
public:
    bool alwaysShow, startMin, onMin, animate, singleClick; CString iconPath; int iconIndex;
    std::function<HICON()> getDefaultIcon;   // the app's own icon, for the preview and for "Default"
    std::function<HICON(const CString&, int)> loadIconFrom;   // extracts an icon from a file at a given index, or nullptr on failure
    CTrayDlg(bool aw, bool sm, bool om, bool an, bool sc, const CString& ip, int ii, CWnd* parent)
        : alwaysShow(aw), startMin(sm), onMin(om), animate(an), singleClick(sc), iconPath(ip), iconIndex(ii) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(230); t.push_back(210);
        t.push_back(0); t.push_back(0); S(L"Tray"); t.push_back(9); S(DEFAULT_FONT);
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 8, 214, 10, IDC_TR_ALWAYS, 0x0080, L"Always show IRC icon in tray");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 22, 214, 10, IDC_TR_STARTMIN, 0x0080, L"On startup minimize IRC to tray");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 36, 214, 10, IDC_TR_ONMIN, 0x0080, L"Place IRC in tray when minimized");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 50, 214, 10, IDC_TR_ANIMATE, 0x0080, L"Animate tray icon on activity");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 64, 214, 10, IDC_TR_SINGLECLICK, 0x0080, L"Single click on tray icon to open");
        Item(SS_CENTER, 8, 86, 90, 9, 0xFFFF, 0x0082, L"Tray icon:");
        Item(SS_NOTIFY, 8, 98, 64, 64, IDC_TR_PREVIEW, 0x0082, L"");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 110, 98, 70, 14, IDC_TR_DEFAULT, 0x0080, L"Default");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 110, 116, 70, 14, IDC_TR_SELECT, 0x0080, L"Select...");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 62, 186, 50, 14, IDOK, 0x0080, L"OK");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 118, 186, 50, 14, IDCANCEL, 0x0080, L"Cancel");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    void RefreshPreview() {
        m_preview.icon = iconPath.IsEmpty() ? (getDefaultIcon ? getDefaultIcon() : nullptr) : (loadIconFrom ? loadIconFrom(iconPath, iconIndex) : nullptr);
        if (m_preview.m_hWnd) m_preview.Invalidate();
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        m_preview.SubclassDlgItem(IDC_TR_PREVIEW, this);
        CheckDlgButton(IDC_TR_ALWAYS, alwaysShow); CheckDlgButton(IDC_TR_STARTMIN, startMin);
        CheckDlgButton(IDC_TR_ONMIN, onMin); CheckDlgButton(IDC_TR_ANIMATE, animate); CheckDlgButton(IDC_TR_SINGLECLICK, singleClick);
        RefreshPreview();
        return TRUE;
    }
    afx_msg void OnDefault() { iconPath.Empty(); iconIndex = 0; RefreshPreview(); }
    afx_msg void OnSelect() {
        CFileDialog fd(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
            L"Icons and programs (*.ico;*.exe;*.dll)|*.ico;*.exe;*.dll|All Files (*.*)|*.*||", this);
        if (fd.DoModal() != IDOK) return;
        iconPath = fd.GetPathName(); iconIndex = 0; RefreshPreview();
    }
    void OnOK() override {
        alwaysShow = IsDlgButtonChecked(IDC_TR_ALWAYS) != 0; startMin = IsDlgButtonChecked(IDC_TR_STARTMIN) != 0;
        onMin = IsDlgButtonChecked(IDC_TR_ONMIN) != 0; animate = IsDlgButtonChecked(IDC_TR_ANIMATE) != 0;
        singleClick = IsDlgButtonChecked(IDC_TR_SINGLECLICK) != 0;
        CDialog::OnOK();
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CTrayDlg, CDialog)
    ON_BN_CLICKED(IDC_TR_DEFAULT, OnDefault) ON_BN_CLICKED(IDC_TR_SELECT, OnSelect)
END_MESSAGE_MAP()

// ---------------- Tips settings dialog (File > Tips...) ----------------
enum { IDC_TP_CHAN = 701, IDC_TP_PRIV, IDC_TP_OTHER, IDC_TP_QSIZE, IDC_TP_DTIME, IDC_TP_FULLSCREEN };
class CTipsDlg : public CDialog {
    std::vector<WORD> t; int cnt = 0;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
public:
    bool chanOn, privOn, otherOn, hideFullscreen; int queueSize, displayTime;
    CTipsDlg(bool c, bool p, bool o, int qs, int dt, bool hf, CWnd* parent)
        : chanOn(c), privOn(p), otherOn(o), hideFullscreen(hf), queueSize(qs), displayTime(dt) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(220); t.push_back(150);
        t.push_back(0); t.push_back(0); S(L"Tips"); t.push_back(9); S(DEFAULT_FONT);
        Item(BS_GROUPBOX, 8, 6, 204, 48, 0xFFFF, 0x0080, L"Events");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 16, 18, 90, 10, IDC_TP_CHAN, 0x0080, L"Channel");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 16, 32, 90, 10, IDC_TP_PRIV, 0x0080, L"Private");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 112, 18, 90, 10, IDC_TP_OTHER, 0x0080, L"Other");
        Item(SS_LEFT, 8, 62, 90, 9, 0xFFFF, 0x0082, L"Queue size:");
        Item(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, 110, 60, 50, 12, IDC_TP_QSIZE, 0x0081, L"");
        Item(SS_LEFT, 8, 78, 90, 9, 0xFFFF, 0x0082, L"Display time (sec):");
        Item(WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, 110, 76, 50, 12, IDC_TP_DTIME, 0x0081, L"");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 96, 204, 20, IDC_TP_FULLSCREEN, 0x0080, L"Hide tips when full screen application is active");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 52, 126, 50, 14, IDOK, 0x0080, L"OK");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 108, 126, 50, 14, IDCANCEL, 0x0080, L"Cancel");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        CheckDlgButton(IDC_TP_CHAN, chanOn); CheckDlgButton(IDC_TP_PRIV, privOn); CheckDlgButton(IDC_TP_OTHER, otherOn);
        CheckDlgButton(IDC_TP_FULLSCREEN, hideFullscreen);
        SetDlgItemInt(IDC_TP_QSIZE, queueSize, FALSE); SetDlgItemInt(IDC_TP_DTIME, displayTime, FALSE);
        return TRUE;
    }
    void OnOK() override {
        chanOn = IsDlgButtonChecked(IDC_TP_CHAN) != 0; privOn = IsDlgButtonChecked(IDC_TP_PRIV) != 0;
        otherOn = IsDlgButtonChecked(IDC_TP_OTHER) != 0; hideFullscreen = IsDlgButtonChecked(IDC_TP_FULLSCREEN) != 0;
        queueSize = (int)GetDlgItemInt(IDC_TP_QSIZE, nullptr, FALSE); displayTime = (int)GetDlgItemInt(IDC_TP_DTIME, nullptr, FALSE);
        if (queueSize < 1) queueSize = 1; if (displayTime < 3) displayTime = 3; if (displayTime > 60) displayTime = 60;
        CDialog::OnOK();
    }
};

// ---------------- Online Timer dialog: current-connection and cumulative connect time (see CMainFrame's OT* members) ----------------
enum { IDC_OT_ENABLE = 641, IDC_OT_CURTIME, IDC_OT_CURDATE, IDC_OT_CURRESET, IDC_OT_TOTTIME, IDC_OT_TOTDATE, IDC_OT_TOTRESET, IDC_OT_SHOWTOTAL };
class COnlineTimerDlg : public CDialog {
    std::vector<WORD> t; int cnt = 0;
    void W(DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); }
    void S(const wchar_t* z) { do t.push_back(*z); while (*z++); }
    void Item(DWORD st, int x, int y, int cx, int cy, WORD id, WORD cls, const wchar_t* txt) {
        if (t.size() & 1) t.push_back(0);
        W(st | WS_CHILD | WS_VISIBLE); W(0);
        t.push_back(x); t.push_back(y); t.push_back(cx); t.push_back(cy); t.push_back(id);
        t.push_back(0xFFFF); t.push_back(cls); S(txt); t.push_back(0); ++cnt;
    }
    void RefreshDisplay() {
        if (getCurrent) SetDlgItemText(IDC_OT_CURTIME, FormatElapsed(getCurrent()));
        if (getTotal) SetDlgItemText(IDC_OT_TOTTIME, FormatElapsed(getTotal()));
        if (getCurrentResetDate) SetDlgItemText(IDC_OT_CURDATE, getCurrentResetDate());
        if (getTotalResetDate) SetDlgItemText(IDC_OT_TOTDATE, getTotalResetDate());
    }
public:
    static CString FormatElapsed(double secs) {
        if (secs < 0) secs = 0;
        int h = (int)(secs / 3600), m = (int)secs / 60 % 60, s = (int)secs % 60;
        CString r; r.Format(L"%02d:%02d:%02d", h, m, s); return r;
    }
    bool enabled = true, showTotal = true;
    std::function<double()> getCurrent, getTotal;
    std::function<CString()> getCurrentResetDate, getTotalResetDate;
    std::function<void()> onResetCurrent, onResetTotal;
    COnlineTimerDlg(CWnd* parent) {
        W(DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU); W(0);
        t.push_back(0); t.push_back(0); t.push_back(0); t.push_back(300); t.push_back(270);
        t.push_back(0); t.push_back(0); S(L"Online Timer"); t.push_back(9); S(DEFAULT_FONT);
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 8, 200, 10, IDC_OT_ENABLE, 0x0080, L"Enable online timer");
        Item(BS_GROUPBOX, 8, 22, 284, 100, 0xFFFF, 0x0080, L"Current connection:");
        Item(SS_LEFT, 16, 36, 60, 10, 0xFFFF, 0x0082, L"Time:");
        Item(SS_CENTER, 16, 48, 268, 12, IDC_OT_CURTIME, 0x0082, L"00:00:00");
        Item(SS_LEFT, 16, 66, 100, 10, 0xFFFF, 0x0082, L"Last reset on:");
        Item(SS_CENTER, 16, 78, 268, 10, IDC_OT_CURDATE, 0x0082, L"");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 220, 100, 60, 14, IDC_OT_CURRESET, 0x0080, L"Reset");
        Item(BS_GROUPBOX, 8, 128, 284, 100, 0xFFFF, 0x0080, L"Total online time:");
        Item(SS_LEFT, 16, 142, 60, 10, 0xFFFF, 0x0082, L"Time:");
        Item(SS_CENTER, 16, 154, 268, 12, IDC_OT_TOTTIME, 0x0082, L"00:00:00");
        Item(SS_LEFT, 16, 172, 100, 10, 0xFFFF, 0x0082, L"Last reset on:");
        Item(SS_CENTER, 16, 184, 268, 10, IDC_OT_TOTDATE, 0x0082, L"");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 220, 206, 60, 14, IDC_OT_TOTRESET, 0x0080, L"Reset");
        Item(BS_AUTOCHECKBOX | WS_TABSTOP, 8, 236, 284, 10, IDC_OT_SHOWTOTAL, 0x0080, L"Show total online time in status windows");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 158, 250, 60, 14, IDOK, 0x0080, L"OK");
        Item(BS_PUSHBUTTON | WS_TABSTOP, 224, 250, 60, 14, IDCANCEL, 0x0080, L"Cancel");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
    }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        CheckDlgButton(IDC_OT_ENABLE, enabled); CheckDlgButton(IDC_OT_SHOWTOTAL, showTotal);
        RefreshDisplay();
        SetTimer(1, 1000, nullptr);
        return TRUE;
    }
    afx_msg void OnTimer(UINT_PTR) { RefreshDisplay(); }
    afx_msg void OnResetCur() { if (onResetCurrent) onResetCurrent(); RefreshDisplay(); }
    afx_msg void OnResetTot() { if (onResetTotal) onResetTotal(); RefreshDisplay(); }
    void OnOK() override { enabled = IsDlgButtonChecked(IDC_OT_ENABLE) != 0; showTotal = IsDlgButtonChecked(IDC_OT_SHOWTOTAL) != 0; KillTimer(1); CDialog::OnOK(); }
    void OnCancel() override { KillTimer(1); CDialog::OnCancel(); }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(COnlineTimerDlg, CDialog)
    ON_WM_TIMER() ON_BN_CLICKED(IDC_OT_CURRESET, OnResetCur) ON_BN_CLICKED(IDC_OT_TOTRESET, OnResetTot)
END_MESSAGE_MAP()

// ---------------- Main frame: connection, protocol, commands ----------------
class CMainFrame : public CMDIFrameWnd {
    std::vector<std::unique_ptr<Net>> m_nets; int m_netSeq = 0; Opts m_defOpts;   // m_defOpts: last-used settings, pre-fills each new Connect dialog
    std::vector<Bookmark> m_bookmarks;   // saved server list (servers.ini)
    std::vector<ChanFav> m_favs;         // saved channel favorites (channels.ini)
    CMenu m_menu; CChanBar m_bar; CSwitchBar m_sw; CToolBar m_tb; CImageList m_tbImg; bool m_swTop = true; LOGFONT m_chatFont = {};
    int m_tbIcon = 16;   // toolbar icon edge in pixels (24 with the resource strip, 16 for the drawn fallback)
    CString m_swSkinPath, m_tbSkinPath, m_mdiSkinPath;   // as stored in the ini: relative to the exe when possible, e.g. "images\skin.png"
    std::unique_ptr<Gdiplus::Bitmap> m_swSkinBmp, m_tbSkinBmp, m_mdiSkinBmp;
    CMdiClient m_mdiWrap;   // the MDI workspace, subclassed once m_hWndMDIClient exists (see Start())
    bool m_logEnabled = false; CString m_logFolder;   // chat history logging (see LoadLogging/SaveLogging/WriteLog)
    bool m_tsGlobalOn = true; CString m_tsEventFmt = L"[HH:nn]", m_tsLogFmt = L"[HH:nn:ss]";   // see /timestamp, LoadTimestamp/SaveTimestamp
    std::vector<PlayItem> m_playQueue; CString m_pnick; UINT_PTR m_playTimerId = 0;   // see /play, /playctrl, PlayTick
    std::vector<DnsRequest> m_dnsQueue; std::map<CString, int> m_pendingUserhost; int m_dnsSeq = 0;
    std::vector<std::pair<CString, CString>> m_lastDnsRecords;   // the most recently completed -m request's records, for $dns(T,N)
    std::vector<TimerInfo> m_timers; CString m_ltimer; UINT_PTR m_timerTickId = 0;   // see /timer, /timers, TimerTick
    // ---- Identd server ----
    bool m_identdEnabled = false, m_identdShowReq = true, m_identdOnlyConnecting = false, m_identdUseEmail = false;
    CString m_identdUserId = L"user", m_identdSystem = L"UNIX"; int m_identdPort = 113;
    std::shared_ptr<IdentdState> m_identdState;
    Net* m_identdTriggerNet = nullptr;   // which network's status window to report requests to, when started via "only when connecting"
    ULONGLONG m_identdAutoStopAt = 0;    // "only when connecting" fallback: stop even if no request ever arrives
    // ---- System tray ----
    bool m_trayAlwaysShow = false, m_trayMinOnStartup = false, m_trayOnMinimize = false, m_trayAnimate = true, m_traySingleClick = false;
    CString m_trayIconPath; int m_trayIconIndex = 0;
    bool m_trayIconAdded = false, m_trayIsCustomIcon = false;
    HICON m_trayIconHandle = nullptr;      // whatever's currently shown (either an extracted custom icon, owned here, or a shared system/app one)
    HICON m_trayAlertIcon = nullptr;       // the simple generated "activity" alternate frame (see MakeTrayAlertIcon)
    bool m_trayFlashOn = false; UINT_PTR m_trayAnimTimerId = 0;
    NOTIFYICONDATAW m_trayNid = {};
    // ---- Tips: balloon notifications near the tray icon (built on the same NOTIFYICONDATAW/tray icon above) ----
    bool m_tipsOn = true, m_tipsChannel = true, m_tipsPrivate = true, m_tipsOther = true, m_tipsHideFullscreen = true;
    int m_tipsQueueSize = 5, m_tipsDisplayTime = 10;
    std::vector<TipInfo> m_tipQueue; int m_tipSeq = 0;
    bool m_tipsAppWasActive = true;
    // ---- Sound playback (/splay, /vol, $vol, $inwave/$inmidi/$insong, $sound) ----
    SoundChannel m_waveChan{ L"ircwave" }, m_midiChan{ L"ircmidi" }, m_mp3Chan{ L"ircmp3" };
    CString m_soundDirWave, m_soundDirMidi, m_soundDirMp3, m_soundDirWma, m_soundDirOgg;
    // ---- Online Timer: current-connection and cumulative connect time (unrelated to the scheduled-command /timer feature above) ----
    bool m_otEnabled = true, m_otShowTotal = true;
    ULONGLONG m_otSessionStart = 0;   // GetTickCount64() when the current unbroken "connected" streak began; 0 = not currently counting
    CTime m_otSessionResetTime = CTime((time_t)0), m_otTotalResetTime = CTime((time_t)0);
    double m_otTotalBanked = 0;       // accumulated seconds from completed sessions (persisted); the live total also adds the current session on top
    std::vector<ColorScheme> m_schemes; int m_curScheme = 0;   // Colors dialog: named schemes, and which one is active (colors.ini... see LoadColors)
    CString m_bt[4]; int m_seqn = 0; std::vector<CMDIChildWnd*> m_tabWnds;   // CChatWnd and CListWnd both live here now
    std::map<CString, CChatWnd*> m_w;
    std::vector<AliasDef> m_aliases; std::vector<CString> m_runStack;   // aliases (aliases.ini) and the alias names currently running
    int m_depth = 0, m_steps = 0; bool m_halt = false; CString m_result, m_prop, m_lastPrompt;   // state of the running script: $result, $prop, $!
    VarMap m_vars; std::vector<VarScope> m_scopes; bool m_varsDirty = false;   // global variables (vars.ini) and the stack of per-script local scopes

    static bool IsChan(const CString& s) { return !s.IsEmpty() && wcschr(L"#&+!", s[0]); }
    static CString Key(Net* net, CString s) { s.MakeLower(); CString k; k.Format(L"%d:", net ? net->id : 0); return k + s; }
    CChatWnd* Find(Net* net, const CString& n) { auto i = m_w.find(Key(net, n)); return i == m_w.end() ? nullptr : i->second; }
    CChatWnd* Status(Net* net) { return Open(net, L"*status*", false); }
    Net* NewNet() {   // a brand-new, independent connection: its own socket, nick, status window and channels
        m_nets.push_back(std::make_unique<Net>());
        Net* net = m_nets.back().get();
        net->id = ++m_netSeq; net->o = m_defOpts; net->nick = m_defOpts.nick.IsEmpty() ? CString(L"User") : m_defOpts.nick;
        WireNet(net);
        return net;
    }
    void WireNet(Net* net) {   // hooks this network's socket callbacks; called once, right after NewNet()
        net->sock.onConn = [this, net](int err) {
            if (err) {
                CString hex; hex.Format(L"0x%08X", (unsigned)err);
                SetState(net, L"Connection failed");
                Note(net, L"Connection/TLS failed (status " + hex + L"). If TLS is on, the most common cause is connecting to a plaintext port; "
                     L"try the server's TLS port (often 6697) instead.", cPart);
                return;
            }
            net->conn = true; Note(net, L"Connected. Registering...");
            SetState(net, L"Connected to " + net->o.host + (net->o.tls ? L" (TLS)" : L"") + L", registering...");
            if (!net->o.pass.IsEmpty()) Send(net, L"PASS " + net->o.pass);
            Send(net, L"NICK " + net->nick); Send(net, L"USER " + net->o.user + L" 0 * :" + net->o.real);
        };
        net->sock.onLine = [this, net](const CString& s) { OnLine(net, s); };
        net->sock.onDrop = [this, net]() { net->conn = false; SetState(net, L"Disconnected"); Note(net, L"Disconnected.", cPart); };
    }
    void Show(CChatWnd* w, const CString& t, COLORREF c = cText, int tsOverride = -1) {
        if (!w) return;
        w->AddLine(t, c, tsOverride);
        if (w != dynamic_cast<CChatWnd*>(MDIGetActive())) w->m_act = (std::max)(w->m_act, (c == cText || c == cAction) ? 2 : 1);
    }
    void Activate(CMDIChildWnd* w) { if (w->IsIconic()) MDIRestore(w); MDIActivate(w); }   // CMDIChildWnd base: works for both CChatWnd and CListWnd
    void Goto(Net* net, const CString& t) {   // switchbar / double-click target: existing window is activated, unknown #chan is joined
        if (IsChan(t)) { if (CChatWnd* w = Find(net, t)) Activate(w); else Send(net, L"JOIN " + t); }
        else Activate(Open(net, t, false));
    }
    CChatWnd* OpenBg(Net* net, const CString& n) {   // incoming PM: open the query but keep focus where it was; its button turns red
        if (CChatWnd* e = Find(net, n)) return e;
        CMDIChildWnd* prev = MDIGetActive(); CChatWnd* w = Open(net, n, false);
        if (prev) MDIActivate(prev);
        return w;
    }
    void Note(Net* net, const CString& t, COLORREF c = cInfo) { Show(Status(net), t, c); }
    void SetState(Net* net, const CString& t) { net->state = t; RefreshBars(); }
    void RefreshBars() {   // switchbar buttons + status bar panes: [server state] [nick] [active window] [channels]
        if (!m_bar.m_hWnd || !m_sw.m_hWnd) return;
        std::vector<CMDIChildWnd*> ws;   // CChatWnd and CListWnd together, so /list windows also get a switchbar button
        for (auto& kv : m_w) ws.push_back(kv.second);
        for (auto& np : m_nets) if (np->listWnd) ws.push_back(np->listWnd);
        auto getNet = [](CMDIChildWnd* w) -> Net* {
            if (auto* c = dynamic_cast<CChatWnd*>(w)) return c->net;
            if (auto* l = dynamic_cast<CListWnd*>(w)) return l->net;
            return nullptr;
        };
        auto getSeq = [](CMDIChildWnd* w) -> int {
            if (auto* c = dynamic_cast<CChatWnd*>(w)) return c->m_seq;
            if (auto* l = dynamic_cast<CListWnd*>(w)) return l->m_seq;
            return 0;
        };
        std::sort(ws.begin(), ws.end(), [&](CMDIChildWnd* x, CMDIChildWnd* y) {   // group by network first, then creation order within it
            Net* nx = getNet(x); Net* ny = getNet(y);
            int nxid = nx ? nx->id : 0, nyid = ny ? ny->id : 0;
            return nxid != nyid ? nxid < nyid : getSeq(x) < getSeq(y);
        });
        CMDIChildWnd* activeAny = MDIGetActive();
        auto* a = dynamic_cast<CChatWnd*>(activeAny);
        if (a) a->m_act = 0;                                   // activity clears once the window is active
        bool multi = m_nets.size() > 1;                        // more than one network: prefix window labels with its tag
        std::vector<CSwitchBar::Btn> bs; CString chans; m_tabWnds = ws;
        for (auto* mw : ws) {
            CSwitchBar::Btn b;
            if (auto* w = dynamic_cast<CChatWnd*>(mw)) {
                CString lbl = w->m_name == L"*status*" ? CString(L"Status") : w->m_name;
                b.text = (multi && w->net) ? (w->net->tag + L": " + lbl) : lbl;
                b.act = w->m_act;
                if (w->m_chan && (!a || w->net == a->net)) chans += w->m_name + L" ";   // only the active window's network
            } else if (auto* lw = dynamic_cast<CListWnd*>(mw)) {
                CString lbl; mw->GetWindowText(lbl);
                b.text = (multi && lw->net) ? (lw->net->tag + L": " + lbl) : lbl;
                b.act = 0;
            }
            b.sel = (mw == activeAny);
            bs.push_back(b);
        }
        m_sw.Set(bs);
        CString act, state, nick;
        if (a && a->net) {
            CString n; n.Format(L"%d", a->NickCount());
            act = a->m_chan ? a->m_name + L": " + n + L" users" : (a->m_name == L"*status*" ? CString(L"Status window") : L"Query: " + a->m_name);
            state = multi ? (a->net->tag + L" - " + a->net->state) : a->net->state; nick = a->net->nick;
        } else state = m_nets.empty() ? CString(L"Not connected") : CString(L"");
        auto set = [&](int i, const CString& t) { if (m_bt[i] != t) { m_bt[i] = t; m_bar.SetPaneText(i, t); } };
        set(0, state); set(1, nick.IsEmpty() ? CString(L"") : L"Nick: " + nick); set(2, act);
        set(3, chans.IsEmpty() ? CString(L"No channels") : L"Channels: " + chans);
    }

    CListWnd* OpenListWnd(Net* net) {   // one /list window per network; a repeat /list reuses and refreshes it
        if (net->listWnd) { Activate(net->listWnd); return net->listWnd; }
        auto* w = new CListWnd(); w->net = net; w->m_seq = ++m_seqn;
        w->onJoin = [this](Net* n, CString chan) { Send(n, L"JOIN " + chan); };
        w->onClosed = [this](CListWnd* c) { for (auto& np : m_nets) if (np->listWnd == c) np->listWnd = nullptr; };
        CString title = (m_nets.size() > 1) ? net->tag + L": Channel List" : CString(L"Channel List");
        w->Create(nullptr, title, WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, rectDefault, this);
        net->listWnd = w;
        return w;
    }
    CChatWnd* Open(Net* net, const CString& name, bool chan) {
        if (auto* e = Find(net, name)) return e;
        BOOL wasMax = FALSE; MDIGetActive(&wasMax);   // a new window should open maximized too, if whatever you're currently looking at already is
        auto* w = new CChatWnd(name, chan);
        w->net = net;
        w->onInput = [this](CChatWnd* c, CString s) { OnInput(c, s); };
        w->onClose = [this](CChatWnd* c) { Forget(c); };
        w->onOpen = [this](CChatWnd* c, CString t) { Goto(c->net, t); };
        w->onNickMenu = [this](CChatWnd* c, CString nick, CPoint pt) { ShowNickMenu(c, nick, pt); };
        w->onLogMenu = [this](CChatWnd* c, CPoint pt) { return ShowWindowPopup(c, pt); };
        w->onLog = [this, net, w](const CString& line) { WriteLog(net, w->m_name, line); };
        w->tsEnabled = [this, w]() { return w->m_tsMode == -1 ? m_tsGlobalOn : (w->m_tsMode == 1); };
        w->tsFormat = [this]() { return m_tsEventFmt; };
        w->m_seq = ++m_seqn;
        w->Create(nullptr, name, WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, rectDefault, this);
        if (wasMax) w->ShowWindow(SW_SHOWMAXIMIZED);
        w->ApplyFont(m_chatFont);
        { const ColorScheme& s = CurScheme(); w->ApplyColors(s.chatBg, s.editBg, s.nickBg); }
        m_w[Key(net, name)] = w;
        return w;
    }
    int m_cwSeq = 0;
    CChatWnd* OpenCustomWindow(const CString& name, bool hidden = false) {   // /window: a separate factory from Open() so status/channel/query windows are never at risk from this
        if (auto* e = Find(nullptr, name)) return e;
        BOOL wasMax = FALSE; MDIGetActive(&wasMax);
        auto* w = new CChatWnd(name, false);
        w->net = nullptr; w->m_custom = true; w->m_hasEdit = false; w->m_cwId = ++m_cwSeq;
        w->onInput = [this](CChatWnd* c, CString s) { OnInput(c, s); };
        w->onClose = [this](CChatWnd* c) { Forget(c); };
        w->onOpen = [this](CChatWnd* c, CString t) { Goto(c->net, t); };
        w->onLogMenu = [this](CChatWnd* c, CPoint pt) { return ShowCustomPopup(c, pt); };
        w->tsEnabled = [this, w]() { return w->m_tsMode == -1 ? m_tsGlobalOn : (w->m_tsMode == 1); };
        w->tsFormat = [this]() { return m_tsEventFmt; };
        w->m_seq = ++m_seqn;
        w->Create(nullptr, name, WS_CHILD | (hidden ? 0 : WS_VISIBLE) | WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, rectDefault, this);
        if (wasMax && !hidden) w->ShowWindow(SW_SHOWMAXIMIZED);
        w->ApplyFont(m_chatFont);
        { const ColorScheme& s = CurScheme(); w->ApplyColors(s.chatBg, s.editBg, s.nickBg); }
        m_w[Key(nullptr, name)] = w;
        return w;
    }
    void Forget(CChatWnd* c) {   // user closed the window
        Net* net = c->net;
        for (auto i = m_w.begin(); i != m_w.end(); ++i)
            if (i->second == c) { if (c->m_chan && net && net->conn) Send(net, L"PART " + c->m_name); m_w.erase(i); break; }
        if (net && c->m_name == L"*status*" && net->conn) { Send(net, L"QUIT :" + CString(VERSION)); net->sock.Close(); net->conn = false; }
    }
    void Drop(Net* net, const CString& n) {  // server-driven close (no PART echo)
        auto i = m_w.find(Key(net, n)); if (i == m_w.end()) return;
        CChatWnd* w = i->second; m_w.erase(i); w->DestroyWindow();
    }
    void Send(Net* net, CString l) {
        if (!net || !net->conn) { Note(net, L"Not connected. Use /server <host> [port]", cPart); return; }
        l.Remove(L'\r'); l.Remove(L'\n');
        CW2A conv(l, CP_UTF8);
        CStringA u((LPCSTR)conv);
        u += "\r\n";
        net->sock.Write(std::string((LPCSTR)u, u.GetLength()));
    }
    void Say(Net* net, const CString& target, const CString& text, bool action = false) {
        CChatWnd* w = Find(net, target); if (!w) w = Open(net, target, IsChan(target));
        if (action) { Send(net, L"PRIVMSG " + target + L" :" + CString(wchar_t(1)) + L"ACTION " + text + CString(wchar_t(1))); Show(w, L"* " + net->nick + L" " + text, cAction); }
        else        { Send(net, L"PRIVMSG " + target + L" :" + text); Show(w, L"<" + net->nick + L"> " + text, cOwn); }
    }
    void Connect(Net* net, const CString& host, UINT port) {
        if (m_identdEnabled && m_identdOnlyConnecting) StartIdentd(net);
        if (net->sock.m_hSocket != INVALID_SOCKET) net->sock.Close();
        net->conn = false; net->network.Empty(); net->chanmodes = L"beI,k,l,imnpst"; net->sock.buf.Empty(); net->sock.sendq.clear();
        delete net->sock.tls; net->sock.tls = nullptr;
        if (net->o.tls) {
            net->sock.tls = new CTls;
            if (!net->sock.tls->Init(host, net->o.lax)) { Note(net, L"TLS initialisation failed", cPart); return; }
        }
        Note(net, L"Connecting to " + host + (net->o.tls ? L" (TLS)" : L"") + L"...");
        SetState(net, L"Connecting to " + host + L"...");
        if (!net->sock.ConnectSmart(host, port))
            Note(net, L"Connect failed", cPart);
    }
    void ShowNickMenuLegacy(CChatWnd* c, const CString& nick, CPoint pt) {   // the built-in Whois / Query / Notice menu, used when [lpopup] is empty
        Net* net = c->net; if (!net) return;
        CMenu m; m.CreatePopupMenu();
        m.AppendMenu(MF_STRING, 1, L"Whois");
        m.AppendMenu(MF_STRING, 2, L"Query (Privmsg)");
        m.AppendMenu(MF_STRING, 3, L"Notice");
        SetForegroundWindow();
        m_menuOpen = true;
        int cmd = m.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
        m_menuOpen = false;
        PostMessage(WM_NULL, 0, 0);
        switch (cmd) {
            case 1: Send(net, L"WHOIS " + nick); break;
            case 2: Activate(Open(net, nick, false)); break;
            case 3: {
                CString txt;
                CPromptDlg d(txt, L"Send Notice", L"Notice to " + nick + L":", this);
                if (d.DoModal() == IDOK && !txt.IsEmpty()) { Send(net, L"NOTICE " + nick + L" :" + txt); Note(net, L"-> -" + nick + L"- " + txt, cNotice); }
                break;
            }
        }
    }
    void AddtoClipboard(CString clipboard) {
        if (AfxGetMainWnd()->OpenClipboard())
        {
            ::EmptyClipboard();

            CString strText = clipboard;
            HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (strText.GetLength() + 1) * sizeof(TCHAR));

            if (hGlobal)
            {
                TCHAR* pDest = (TCHAR*)GlobalLock(hGlobal);
                _tcscpy_s(pDest, strText.GetLength() + 1, strText);
                GlobalUnlock(hGlobal);

                // Use CF_UNICODETEXT for Unicode builds, CF_TEXT for ANSI
                SetClipboardData(CF_UNICODETEXT, hGlobal);
            }

            CloseClipboard();
        }
    }

    // ---- identifiers: $me $chan $network $os $date $adate $day $daylight $fulldate $gmt $time, and $0 $N $N- $N-M ----
    // Evaluated by "//cmd ..." and inside aliases (like mIRC: a plain "/cmd" line is NOT evaluated).
    bool IdentValue(CChatWnd* w, const CString& name, const CString& prop, CString& val) {
        Net* net = w ? w->net : nullptr;
        CTime now = CTime::GetCurrentTime();
        if (name == L"me") { val = net ? net->nick : CString(); return true; }
        if (name == L"pnick") { val = m_pnick; return true; }   // the nick/channel /play is currently sending to
        if (name == L"ltimer") { val = m_ltimer; return true; }   // the id of the last timer started by /timer
        if (name == L"tips") { val = m_tipsOn ? L"$true" : L"$false"; return true; }
        if (name == L"inwave" || name == L"inmidi" || name == L"insong") {   // $inwave.fname / .pos / .length / .pause (bare identifier, no parens -- see the EvalIds property-parsing fix above)
            SoundChannel& ch = name == L"inwave" ? m_waveChan : name == L"inmidi" ? m_midiChan : m_mp3Chan;
            if (prop.IsEmpty()) { val = (ch.open && ch.playing) ? L"$true" : L"$false"; return true; }
            if (prop == L"fname") val = ch.curFile;
            else if (prop == L"pos") val = ch.open ? MciCmd(L"status " + ch.alias + L" position") : CString(L"0");
            else if (prop == L"length") val = ch.open ? MciCmd(L"status " + ch.alias + L" length") : CString(L"0");
            else if (prop == L"pause") val = ch.paused ? L"$true" : L"$false";
            else val = (ch.open && ch.playing) ? L"$true" : L"$false";
            return true;
        }
        if (name == L"titlebar") {   // the active chat window's own title (not the main app titlebar)
            CMDIChildWnd* act = MDIGetActive();
            if (act) act->GetWindowText(val); else val.Empty();
            return true;
        }
        if (name == L"null") { val.Empty(); return true; }
        if (name == L"server") { val = (net && net->conn) ? net->o.host : CString(); return true; }   // empty ($null) when not connected
        if (name == L"menu" || name == L"menutype" || name == L"menucontext") { val = m_menuType; return true; }   // which popup is being built: status channel query nicklist menubar
        if (name == L"prop") { val = m_prop; return true; }         // the .property used to call a custom identifier: $add(1,2).negative
        if (name == L"result") { val = m_result; return true; }     // what the last alias/identifier "return"ed
        if (name == L"error") { val.Empty(); return true; }
        if (name == L"true") { val = L"1"; return true; }
        if (name == L"false") { val = L"0"; return true; }
        if (name == L"ticks") { val.Format(L"%I64u", (unsigned __int64)GetTickCount64()); return true; }
        if (name == L"chan") { val = (w && w->m_chan) ? w->m_name : CString(); return true; }
        if (name == L"network") { val = net ? net->network : CString(); return true; }
        if (name == L"os") { val = OsName(); return true; }
        if (name == L"date") { val = now.Format(L"%d/%m/%Y"); return true; }
        if (name == L"adate") { val = now.Format(L"%m/%d/%Y"); return true; }
        if (name == L"day") { val = now.Format(L"%A"); return true; }
        if (name == L"fulldate") { val = now.Format(L"%a %b %d %H:%M:%S %Y"); return true; }
        if (name == L"time") { val = now.Format(L"%H:%M:%S"); return true; }
        if (name == L"gmt") { val.Format(L"%I64d", (__int64)now.GetTime()); return true; }   // seconds since 1970, UTC-based
        if (name == L"version") { val.Format(VERSION); return true; }
        if (name == L"daylight") {
            TIME_ZONE_INFORMATION tz = {}; DWORD id = GetTimeZoneInformation(&tz);
            val.Format(L"%ld", id == TIME_ZONE_ID_DAYLIGHT ? -tz.DaylightBias * 60 : 0L);   // seconds of DST offset, 0 when not in effect
            return true;
        }
        return false;
    }
    // ---- variables ----
    static CString VKey(CString s) { s.MakeLower(); return s; }
    struct VarSw { bool g = false, l = false, n = false, e = false, i = false, k = false, p = false, z = false, c = false; long u = -1; };
    struct ScopeGuard {   // one variable scope per script run, so /var locals vanish when it ends
        CMainFrame* f; bool on;
        ScopeGuard(CMainFrame* p, bool push) : f(p), on(push) { if (on) f->m_scopes.emplace_back(); }
        ~ScopeGuard() { if (on) f->PopScope(); }
    };
    void PopScope() {
        VarScope sc = std::move(m_scopes.back()); m_scopes.pop_back();
        for (auto& k : sc.unsetAtEnd) if (m_vars.erase(k)) m_varsDirty = true;   // /set -u0 globals go away when the script ends
        FlushVars();
    }
    void FlushVars() { if (m_varsDirty) { m_varsDirty = false; SaveVars(); } }
    void LoadVars() {   // vars.ini:  [variables]  n0=%name value
        m_vars.clear();
        CString path = IniPath(L"vars.ini");
        std::vector<wchar_t> buf(65536, 0);
        DWORD n = GetPrivateProfileSectionW(L"variables", buf.data(), (DWORD)buf.size(), path);
        if (!n) return;
        for (wchar_t* p = buf.data(); *p; p += wcslen(p) + 1) {
            CString line = p; int eq = line.Find(L'=');
            if (eq <= 0) continue;
            CString v = line.Mid(eq + 1); int sp = v.Find(L' ');
            CString name = sp < 0 ? v : v.Left(sp), val = sp < 0 ? CString() : v.Mid(sp + 1);
            if (name.Left(1) != L"%") continue;
            VarEntry e; e.name = name; e.value = val; m_vars[VKey(name)] = e;
        }
    }
    void SaveVars() {
        CString path = IniPath(L"vars.ini");
        WritePrivateProfileStringW(L"variables", nullptr, nullptr, path);   // drop the section, then rewrite it in order
        int i = 0;
        for (auto& kv : m_vars) {
            if (kv.second.noSave) continue;   // -e: lives only until the program exits
            CString key; key.Format(L"n%d", i++);
            WritePrivateProfileStringW(L"variables", key, kv.second.name + L" " + kv.second.value, path);
        }
    }
    VarEntry* FindVar(const CString& name, bool loc = true, bool glob = true) {
        CString k = VKey(name);
        if (loc && !m_scopes.empty()) { auto it = m_scopes.back().locals.find(k); if (it != m_scopes.back().locals.end()) return &it->second; }
        if (glob) { auto it = m_vars.find(k); if (it != m_vars.end()) return &it->second; }
        return nullptr;
    }
    CString GetVar(const CString& name) { VarEntry* v = FindVar(name); return v ? v->value : CString(); }   // a variable that isn't set is $null (empty)
    void TickVars() {   // off the frame timer: -uN expiry, and the once-a-second stepping of -c / -z
        if (m_vars.empty()) return;
        ULONGLONG now = GetTickCount64(); bool changed = false;
        for (auto it = m_vars.begin(); it != m_vars.end();) {
            VarEntry& e = it->second; bool gone = false;
            if (e.step != 0) {
                for (int guard = 0; now >= e.nextStep && guard < 3600; guard++) {
                    double cur = 0; ParseNum(e.value, cur); cur += e.step; e.value = FmtNum(cur); e.nextStep += 1000;
                    if (e.zeroUnset && cur <= 0) { gone = true; break; }
                }
            }
            if (!gone && e.expire && now >= e.expire) gone = true;
            if (gone) { it = m_vars.erase(it); changed = true; } else ++it;
        }
        if (changed) SaveVars();
    }
    // Creates or updates a variable. forceLocal (/var) goes into the current script scope; otherwise it updates a same-named
    // local if there is one, else the global. -g forces global, -l forces local.
    VarEntry* StoreVar(const CString& name, const CString& value, const VarSw& sw, bool forceLocal) {
        CString k = VKey(name);
        VarMap* target = &m_vars; bool isLocal = false;
        if (!m_scopes.empty()) {
            if (forceLocal || sw.l) { target = &m_scopes.back().locals; isLocal = true; }
            else if (!sw.g && m_scopes.back().locals.count(k)) { target = &m_scopes.back().locals; isLocal = true; }
        }
        auto it = target->find(k); bool existed = it != target->end();
        if (existed && sw.i) return &it->second;   // -i: only initialise when it doesn't exist yet
        VarEntry& e = (*target)[k];
        if (!existed) e.name = name;
        else if (!sw.k) { e.expire = 0; e.step = 0; e.zeroUnset = false; }   // setting it again cancels a pending -uN / countdown, unless -k
        e.value = value;
        if (sw.e) e.noSave = true; else if (!sw.k) e.noSave = false;
        ULONGLONG now = GetTickCount64();
        if (sw.u > 0) e.expire = now + (ULONGLONG)sw.u * 1000ULL;
        else if (sw.u == 0 && !isLocal && !m_scopes.empty()) m_scopes.back().unsetAtEnd.push_back(k);   // -u0: when the script finishes
        if (sw.z) { e.step = -1; e.zeroUnset = true; e.nextStep = now + 1000; }
        if (!isLocal) m_varsDirty = true;
        return &e;
    }
    int UnsetMatching(const CString& pat, bool g, bool l) {   // an exact %name or a wildcard pattern; returns how many were removed
        int n = 0; bool wild = pat.FindOneOf(L"*?") >= 0; CString pk = VKey(pat);
        auto sweep = [&](VarMap& m, bool isGlobal) {
            for (auto it = m.begin(); it != m.end();) {
                if (wild ? GlobMatch(pk, it->first) : it->first == pk) { it = m.erase(it); n++; if (isGlobal) m_varsDirty = true; }
                else ++it;
            }
        };
        if (l && !m_scopes.empty()) sweep(m_scopes.back().locals, false);
        if (g) sweep(m_vars, true);
        return n;
    }
    // Leading "-xyz" switch tokens; 'allowed' lists the letters this command accepts. Returns false (after reporting) on a bad one.
    bool ParseVarSw(CChatWnd* w, const CString& cmd, CString& a, VarSw& sw, const wchar_t* allowed) {
        for (;;) {
            CString t = a; t.TrimLeft();
            if (t.Left(1) != L"-" || t.GetLength() < 2) break;
            CString tok = Word(a);
            for (int i = 1; i < tok.GetLength(); i++) {
                wchar_t c = (wchar_t)towlower(tok[i]);
                if (!wcschr(allowed, c)) { Show(w, L"* /" + cmd + L": unknown switch -" + CString(c), cPart); return false; }
                switch (c) {
                case L'g': sw.g = true; break;
                case L'l': sw.l = true; break;
                case L'n': sw.n = true; break;
                case L'e': sw.e = true; break;
                case L'i': sw.i = true; break;
                case L'k': sw.k = true; break;
                case L'p': sw.p = true; break;
                case L'z': sw.z = true; break;
                case L'c': sw.c = true; break;
                case L'u': { long v = 0; bool any = false; while (i + 1 < tok.GetLength() && iswdigit(tok[i + 1]) && v < 100000000) { v = v * 10 + (tok[++i] - L'0'); any = true; } sw.u = any ? v : 0; break; }
                default: break;   // -s is accepted and ignored
                }
            }
        }
        return true;
    }
    // The value given to /set, /var and "%x = ...": trimmed (unless -p), and a single "5 + 1" style operation is worked out (unless -n).
    bool FinalValue(CChatWnd* w, const CString& cmd, CString v, const VarSw& sw, CString& out) {
        if (!sw.p) v.Trim();
        out = v;
        if (!sw.n) {
            CString r; int m = TryMath(v, r);
            if (m < 0) { Show(w, L"* /" + cmd + L": division by zero", cPart); return false; }
            if (m > 0) out = r;
        }
        return true;
    }
    void ListVars(CChatWnd* w) {
        if (m_vars.empty()) { Show(w, L"* No variables set. Usage: /set %name value", cInfo); return; }
        for (auto& kv : m_vars) Show(w, L"* " + kv.second.name + L" = " + kv.second.value, cInfo);
    }
    void CmdSet(CChatWnd* w, CString arg) {
        VarSw sw; if (!ParseVarSw(w, L"set", arg, sw, L"snzeglkipu")) return;
        CString name = Word(arg);
        if (name.IsEmpty()) { ListVars(w); return; }
        if (name[0] != L'%') { Show(w, L"* /set: variable names start with %  (e.g. /set %test 1)", cPart); return; }
        CString v; if (!FinalValue(w, L"set", arg, sw, v)) return;
        StoreVar(name, v, sw, false); FlushVars();
    }
    void CmdVar(CChatWnd* w, CString arg) {   // /var %x = hello, %y, %z = $me   (local to this script run)
        VarSw sw; if (!ParseVarSw(w, L"var", arg, sw, L"snzeglkipu")) return;
        int pos = 0;
        for (CString item = arg.Tokenize(L",", pos); !item.IsEmpty(); item = arg.Tokenize(L",", pos)) {
            item.Trim(); if (item.IsEmpty()) continue;
            int k = 0; while (k < item.GetLength() && item[k] != L' ' && item[k] != L'=') k++;
            CString name = item.Left(k), tail = item.Mid(k), val; tail.TrimLeft();
            if (name.Left(1) != L"%") { Show(w, L"* /var: variable names start with %", cPart); return; }
            if (tail.Left(1) == L"=") { if (!FinalValue(w, L"var", tail.Mid(1), sw, val)) return; }
            StoreVar(name, val, sw, true);
        }
        FlushVars();
    }
    void CmdUnset(CChatWnd* w, CString arg) {
        VarSw sw; if (!ParseVarSw(w, L"unset", arg, sw, L"sgl")) return;
        bool g = sw.g || !sw.l, l = sw.l || !sw.g;   // neither switch: both scopes; -g: global only; -l: local only
        int pos = 0;
        for (CString n = arg.Tokenize(L" ", pos); !n.IsEmpty(); n = arg.Tokenize(L" ", pos)) UnsetMatching(n, g, l);
        FlushVars();
    }
    void CmdIncDec(CChatWnd* w, const CString& cmd, CString arg, int sign) {
        VarSw sw; if (!ParseVarSw(w, cmd, arg, sw, L"cszeu")) return;
        CString name = Word(arg);
        if (name.Left(1) != L"%") { Show(w, L"* /" + cmd + L": expected a %variable", cPart); return; }
        double amt = 1; arg.Trim();
        if (!arg.IsEmpty() && !ParseNum(arg, amt)) { Show(w, L"* /" + cmd + L": the amount must be a number", cPart); return; }
        double cur = 0; if (VarEntry* ex = FindVar(name)) ParseNum(ex->value, cur);
        VarSw st = sw; st.k = true;   // an existing -uN timer keeps running through /inc and /dec unless a new -uN replaces it
        VarEntry* e = StoreVar(name, FmtNum(cur + sign * amt), st, false);
        if (e && sw.c) { e->step = sign * amt; e->nextStep = GetTickCount64() + 1000; }   // -c: keep stepping once a second
        FlushVars();
    }
    void CmdAssign(CChatWnd* w, const CString& name, CString arg) {   // "%x = 5 + 1"  (arg starts at the '=')
        arg.TrimLeft();
        if (arg.Left(1) != L"=") { Show(w, L"* Expected:  " + name + L" = value", cPart); return; }
        VarSw sw; CString v; if (!FinalValue(w, L"set", arg.Mid(1), sw, v)) return;
        StoreVar(name, v, sw, false); FlushVars();
    }

    // ---- $functions(...) : $calc $round $int $chr $var ----
    // Returns false if 'name' isn't one (or its arguments are unusable), so the text is left exactly as typed.
    bool FuncValue(CChatWnd* w, const CString& name, const CString& rawArgs, const CString& prop, const CString& params, CString& val) {
        if (name == L"calc") { double r; if (!CalcExpr(EvalIds(w, rawArgs, params), r)) return false; val = FmtNum(r); return true; }
        if (name == L"int") { double x; if (!ParseNum(EvalIds(w, rawArgs, params), x)) return false; val = FmtNum(x < 0 ? ceil(x) : floor(x)); return true; }
        if (name == L"round") {
            CString a = EvalIds(w, rawArgs, params); int c = a.ReverseFind(L','); double x = 0, dg = 0;
            if (c < 0) { if (!ParseNum(a, x)) return false; }
            else if (!ParseNum(a.Left(c), x) || !ParseNum(a.Mid(c + 1), dg)) return false;
            int ddig = (int)dg; if (ddig < 0) ddig = 0; if (ddig > 5) ddig = 5;   // 5 decimals is the limit
            double pw = pow(10.0, ddig);
            val = FmtNum((x < 0 ? -1.0 : 1.0) * floor(fabs(x) * pw + 0.5) / pw); return true;
        }
        if (name == L"chr") { double x; if (!ParseNum(EvalIds(w, rawArgs, params), x) || x < 1 || x > 65535) return false; val = CString((wchar_t)(int)x); return true; }
        if (name == L"tip") {   // $tip(name,title,text[,delay,iconfn,iconpos,alias,wid]) creates/replaces a tip; $tip(name/N) queries one
            CString a = EvalIds(w, rawArgs, params);
            std::vector<CString> parts; { int start = 0; while (start <= a.GetLength()) { int c = a.Find(L',', start); if (c < 0) { parts.push_back(a.Mid(start)); break; } parts.push_back(a.Mid(start, c - start)); start = c + 1; } }
            for (auto& p : parts) p.Trim();
            if (parts.size() >= 3 && !parts[0].IsEmpty() && !parts[1].IsEmpty() && !parts[2].IsEmpty()) {   // the CREATE form
                TipInfo t; t.seq = ++m_tipSeq;
                t.name = parts[0]; t.title = parts[1]; t.text = parts[2];
                t.delaySec = parts.size() > 3 && !parts[3].IsEmpty() ? _wtoi(parts[3]) : m_tipsDisplayTime;
                t.iconFn = parts.size() > 4 ? parts[4] : CString();
                t.iconPos = parts.size() > 5 && !parts[5].IsEmpty() ? _wtoi(parts[5]) : 0;
                t.alias = parts.size() > 6 ? parts[6] : CString();
                t.wid = parts.size() > 7 && !parts[7].IsEmpty() ? _wtoi(parts[7]) : (w ? w->m_seq : 0);
                m_tipQueue.erase(std::remove_if(m_tipQueue.begin(), m_tipQueue.end(), [&](const TipInfo& x) { return x.name.CompareNoCase(t.name) == 0; }), m_tipQueue.end());
                AddTipToQueue(t);
                int pos = 0; for (size_t i = 0; i < m_tipQueue.size(); i++) if (m_tipQueue[i].name.CompareNoCase(t.name) == 0) { pos = (int)i + 1; break; }
                val.Format(L"%d", pos);
                return true;
            }
            // the QUERY form: $tip(name/N)
            TipInfo* found = nullptr;
            double idxD;
            if (ParseNum(a, idxD)) { int idx = (int)idxD; if (idx >= 1 && idx <= (int)m_tipQueue.size()) found = &m_tipQueue[idx - 1]; }
            else for (auto& x : m_tipQueue) if (x.name.CompareNoCase(a) == 0) { found = &x; break; }
            if (!found) { val.Empty(); return true; }
            if (prop == L"name") val = found->name; else if (prop == L"title") val = found->title; else if (prop == L"text") val = found->text;
            else if (prop == L"delay") val.Format(L"%d", found->delaySec); else if (prop == L"iconfn") val = found->iconFn;
            else if (prop == L"iconpos") val.Format(L"%d", found->iconPos); else if (prop == L"alias") val = found->alias;
            else if (prop == L"wid") val.Format(L"%d", found->wid); else val = found->name;
            return true;
        }
        if (name == L"vol") {   // $vol(wave|midi|song|master), with .mute
            CString a = EvalIds(w, rawArgs, params); a.MakeLower(); a.Trim();
            int v = 0; bool mute = false;
            if (a == L"wave") { DWORD vol = 0; ::waveOutGetVolume(nullptr, &vol); v = LOWORD(vol); }
            else if (a == L"midi") { CString r = MciCmd(L"status " + m_midiChan.alias + L" volume"); v = r.IsEmpty() ? 0 : (_wtoi(r) * 65535 / 1000); }
            else if (a == L"song") { CString r = MciCmd(L"status " + m_mp3Chan.alias + L" volume"); v = r.IsEmpty() ? 0 : (_wtoi(r) * 65535 / 1000); }
            else if (a == L"master") { v = GetMasterVolumeNow(); mute = GetMasterMuteNow(); }
            else { val.Empty(); return true; }
            //val = prop == L"mute" ? (mute ? L"$true" : L"$false") : CString();
            val = (prop == L"mute") ? (mute ? CString(L"$true") : CString(L"$false")) : CString();
            if (prop != L"mute") val.Format(L"%d", v);
            return true;
        }
        if (name == L"sound") {   // $sound(type) -> that type's configured folder; $sound(filename) -> ID3v1 tag properties (mp3 only) or .length via a temporary MCI probe
            CString a = EvalIds(w, rawArgs, params); a.Trim();
            CString a0l = a; a0l.MakeLower();
            if (a0l == L"wave") { val = m_soundDirWave; return true; }
            if (a0l == L"midi") { val = m_soundDirMidi; return true; }
            if (a0l == L"mp3") { val = m_soundDirMp3; return true; }
            if (a0l == L"wma") { val = m_soundDirWma; return true; }
            if (a0l == L"ogg") { val = m_soundDirOgg; return true; }
            if (prop == L"length") {
                CString type = MciTypeForFile(a); val.Empty();
                if (!type.IsEmpty()) {
                    CString resolved = ResolveSoundPath(a); CString cmd; cmd.Format(L"open \"%s\" type %s alias ircprobe", (LPCWSTR)resolved, (LPCWSTR)type);
                    if (MciOk(cmd)) { val = MciCmd(L"status ircprobe length"); MciOk(L"close ircprobe"); }
                }
                return true;
            }
            Id3v1Tag tag; bool haveTag = a0l.Right(4) == L".mp3" && ReadId3v1(ResolveSoundPath(a), tag);
            if (prop == L"title") val = haveTag ? tag.title : CString();
            else if (prop == L"artist") val = haveTag ? tag.artist : CString();
            else if (prop == L"album") val = haveTag ? tag.album : CString();
            else if (prop == L"year") val = haveTag ? tag.year : CString();
            else if (prop == L"comment") val = haveTag ? tag.comment : CString();
            else if (prop == L"genre") val = haveTag ? tag.genre : CString();
            else if (prop == L"track") val = haveTag ? tag.track : CString();
            else val = a;
            return true;
        }
        if (name == L"window") {   // $window(N) or $window(@name) or $window(@wildcard,N): a reduced property set (see the /window notes for what's not modeled here)
            CString a = EvalIds(w, rawArgs, params); int c = a.Find(L',');
            CString sel = c < 0 ? a : a.Left(c); sel.Trim();
            double nn = 1; if (c >= 0 && !ParseNum(a.Mid(c + 1), nn)) return false;
            std::vector<CChatWnd*> customs; for (auto& kv : m_w) if (kv.second->m_custom) customs.push_back(kv.second);
            CChatWnd* cw = nullptr;
            if (!sel.IsEmpty() && sel[0] == L'@') { int idx = 0; for (auto* x : customs) if (GlobMatch(sel, x->m_name) && ++idx == (int)nn) { cw = x; break; } }
            else { double idxD; if (ParseNum(sel, idxD)) { int idx = (int)idxD; if (idx >= 1 && idx <= (int)customs.size()) cw = customs[idx - 1]; } }
            if (!cw) { val.Empty(); return true; }
            CRect r; cw->GetWindowRect(r); ::MapWindowPoints(nullptr, m_hWndMDIClient, (LPPOINT)&r, 2);
            if (prop == L"x") val.Format(L"%d", r.left);
            else if (prop == L"y") val.Format(L"%d", r.top);
            else if (prop == L"w") val.Format(L"%d", r.Width());
            else if (prop == L"h") val.Format(L"%d", r.Height());
            else if (prop == L"title" || prop == L"fulltitle") val = cw->m_name;
            else if (prop == L"state") val = cw->IsIconic() ? L"minimized" : cw->IsZoomed() ? L"maximized" : cw->IsWindowVisible() ? L"normal" : L"hidden";
            else if (prop == L"mdi") val = L"$true";
            else if (prop == L"type") val = cw->m_cwListMode ? L"listbox" : L"text";
            else if (prop == L"wid") val.Format(L"%d", cw->m_cwId);
            else if (prop == L"hwnd") val.Format(L"%zu", (size_t)cw->GetSafeHwnd());
            else if (prop == L"anysc") val = cw->m_cwAnysc ? L"$true" : L"$false";
            else if (prop == L"lb") val = cw->m_cwListMode ? L"1" : L"0";
            else val = cw->m_name;   // default: the name itself
            return true;
        }
        if (name == L"line" || name == L"sline") {   // $line(@name,N) / $sline(@name,N): .state .color for $line; .ln for $sline
            CString a = EvalIds(w, rawArgs, params); int c = a.Find(L',');
            if (c < 0) return false;
            CString wn = a.Left(c); wn.Trim();
            double nn; if (!ParseNum(a.Mid(c + 1), nn)) return false;
            CChatWnd* cw = Find(nullptr, wn);
            if (!cw || !cw->m_custom) { val.Empty(); return true; }
            int idx = (int)nn;
            if (name == L"sline") {   // only a single "selected" line exists here (see m_cwSelectedLine), so N must be 1 (or 0 for the count)
                if (idx == 0) { val = cw->m_cwSelectedLine >= 1 ? L"1" : L"0"; return true; }
                if (idx != 1 || cw->m_cwSelectedLine < 1) { val.Empty(); return true; }
                val = prop == L"ln" ? CString(std::to_wstring(cw->m_cwSelectedLine).c_str()) : cw->m_cwLines[cw->m_cwSelectedLine - 1];
                return true;
            }
            if (idx == 0) { val.Format(L"%d", (int)cw->m_cwLines.size()); return true; }
            if (idx < 1 || idx > (int)cw->m_cwLines.size()) { val.Empty(); return true; }
            if (prop == L"state") val = cw->m_cwSelectedLine == idx ? L"1" : L"0";
            else if (prop == L"color") { COLORREF cr = (size_t)idx <= cw->m_cwColors.size() ? cw->m_cwColors[idx - 1] : cText; val.Format(L"%d", (int)cr); }
            else val = cw->m_cwLines[idx - 1];
            return true;
        }
        if (name == L"dns") {   // $dns(T,N): T is a record type (A/AAAA/NS/MX/SOA/SRV/TXT) or * for all, from the last /dns -m request
            CString a = EvalIds(w, rawArgs, params); int c = a.Find(L',');
            CString T = c < 0 ? a : a.Left(c); T.Trim(); T.MakeUpper();
            double nn = 1; if (c >= 0 && !ParseNum(a.Mid(c + 1), nn)) return false;
            std::vector<CString> matches;
            for (auto& rec : m_lastDnsRecords) if (T == L"*" || rec.first.CompareNoCase(T) == 0) matches.push_back(rec.second);
            int idx = (int)nn;
            if (idx < 1 || idx > (int)matches.size()) { val.Empty(); return true; }
            val = matches[idx - 1]; return true;
        }
        if (name == L"play") {   // $play(N) or $play(Nick,N): .type .fname .topic .pos .lines .delay .status
            CString a = EvalIds(w, rawArgs, params); int c = a.ReverseFind(L',');
            CString who = c < 0 ? CString() : a.Left(c); who.Trim();
            double nn = 1; if (!ParseNum(c < 0 ? a : a.Mid(c + 1), nn)) return false;
            std::vector<size_t> hits;
            for (size_t i = 0; i < m_playQueue.size(); i++) if (who.IsEmpty() || m_playQueue[i].target.CompareNoCase(who) == 0) hits.push_back(i);
            int idx = (int)nn;
            if (idx < 1 || idx > (int)hits.size()) { val.Empty(); return true; }
            PlayItem& it = m_playQueue[hits[idx - 1]];
            if (prop == L"fname") val = it.fname;
            else if (prop == L"topic") val = it.topic;
            else if (prop == L"pos") val.Format(L"%d", (int)it.pos);
            else if (prop == L"lines") val.Format(L"%d", (int)it.lines.size());
            else if (prop == L"delay") val.Format(L"%d", it.delay);
            else if (prop == L"status") val = it.Status();
            else val = it.asCmd ? L"c" : (!it.alias.IsEmpty() ? L"a" : (it.notice ? L"n" : L"m"));   // .type: c=command, a=alias, n=notice, m=message
            return true;
        }
        if (name == L"var") {   // $var(%pattern,N)  N=0 -> how many match;  .value  .local  .secs
            int c = rawArgs.ReverseFind(L',');
            CString pat = c < 0 ? rawArgs : rawArgs.Left(c); pat.Trim();
            double nn = 1; if (c >= 0 && !ParseNum(EvalIds(w, rawArgs.Mid(c + 1), params), nn)) return false;
            CString pk = VKey(pat); bool wild = pk.FindOneOf(L"*?") >= 0;
            std::vector<std::pair<VarEntry*, bool>> hits;
            if (!m_scopes.empty()) for (auto& kv : m_scopes.back().locals) if (wild ? GlobMatch(pk, kv.first) : kv.first == pk) hits.push_back({ &kv.second, true });
            for (auto& kv : m_vars) {
                if (!(wild ? GlobMatch(pk, kv.first) : kv.first == pk)) continue;
                if (!m_scopes.empty() && m_scopes.back().locals.count(kv.first)) continue;   // shadowed by a local of the same name
                hits.push_back({ &kv.second, false });
            }
            int idx = (int)nn;
            if (idx == 0) { val.Format(L"%d", (int)hits.size()); return true; }
            if (idx < 1 || idx > (int)hits.size()) { val.Empty(); return true; }
            VarEntry* e = hits[idx - 1].first; bool loc = hits[idx - 1].second;
            if (prop == L"value") val = e->value;
            else if (prop == L"local") val = loc ? L"1" : L"0";
            else if (prop == L"secs") { ULONGLONG now = GetTickCount64(); val.Format(L"%I64u", e->expire > now ? (e->expire - now) / 1000 : 0ULL); }
            else val = e->name;
            return true;
        }
        if (name == L"iif") {   // $iif(condition,then[,else]): only the branch that is taken gets evaluated
            std::vector<CString> parts; CString cur; int depth = 0;
            for (int k = 0; k < rawArgs.GetLength(); k++) {
                wchar_t ch = rawArgs[k];
                if (ch == L'(') depth++; else if (ch == L')' && depth > 0) depth--;
                if (ch == L',' && depth == 0 && parts.size() < 2) { parts.push_back(cur); cur.Empty(); } else cur += ch;
            }
            parts.push_back(cur);
            if (parts.size() < 2) return false;
            CString pick = EvalCond(w, parts[0], params) ? parts[1] : (parts.size() > 2 ? parts[2] : CString());
            pick.Trim(); val = EvalIds(w, pick, params); return true;
        }
        if (name == L"style") {   // $style(N) first in a popup item: 1 = checked, 2 = disabled, 3 = both
            double x; if (!ParseNum(EvalIds(w, rawArgs, params), x) || x < 1 || x > 3) return false;
            val.Format(L"%c%d%c", 0x1E, (int)x, 0x1E); return true;
        }
        if (!w && FindAlias(name)) return false;   // no window to run a script in (the menu bar being built): leave it as typed
        if (AliasDef* ad = FindAlias(name)) {   // a user alias used as an identifier: $add(1,2) runs /add with $1=1 $2=2 and gives its "return" value
            if (OnRunStack(ad->name)) return false;
            CString a = EvalIds(w, rawArgs, params), plist; int pos = 0;
            for (CString piece = a.Tokenize(L",", pos); !piece.IsEmpty(); piece = a.Tokenize(L",", pos)) { piece.Trim(); plist += (plist.IsEmpty() ? CString() : CString(L" ")) + piece; }
            m_result.Empty();
            RunAlias(w, *ad, plist, prop);
            val = m_result; return true;
        }
        return false;
    }

    // 'params' is the "$1-" line: empty for a hand-typed //command, the alias arguments when an alias runs.
    // Replaces $identifiers, $func(...), $N/$N-/$N-M/$0, %variables (unless evalVars is false), and in scripts also:
    //   #  on its own = the channel you're in;  #$1 = $1 with a # in front unless it already has one
    //   $$x  like $x, but if it comes out empty the whole script halts        $?  $?="text"  $?1  ask for a value ($$? = required)
    //   $!  the last value typed into a $? box                                  $+  glue the text on either side together
    CString EvalIds(CChatWnd* w, const CString& in, const CString& params, bool evalVars = true) {
        std::vector<int> ts, te;   // start/end offsets of each space-delimited token of params
        int pn = params.GetLength();
        for (int i = 0; i < pn;) {
            while (i < pn && params[i] == L' ') i++;
            if (i >= pn) break;
            int st = i; while (i < pn && params[i] != L' ') i++;
            ts.push_back(st); te.push_back(i);
        }
        auto slice = [&](int a, int b) -> CString {   // tokens a..b (1-based, inclusive), verbatim text; b < 0 = to the end
            int cnt = (int)ts.size();
            if (b < 0 || b > cnt) b = cnt;
            if (a < 1 || a > b) return CString();
            return params.Mid(ts[a - 1], te[b - 1] - ts[a - 1]);
        };
        CString out; int L = in.GetLength(); bool hashPending = false;
        for (int i = 0; i < L; i++) {
            wchar_t c = in[i];
            if (c == L'#') {
                bool startsWord = (i == 0 || in[i - 1] == L' '), endsWord = (i + 1 >= L || in[i + 1] == L' ');
                if (startsWord && endsWord && w && w->m_chan) { out += w->m_name; continue; }
                if (i + 1 < L && in[i + 1] == L'$') { hashPending = true; continue; }   // decided once we know the value that follows
                out += c; continue;
            }
            if (c == L'%' && evalVars && i + 1 < L && (iswalnum(in[i + 1]) || in[i + 1] == L'_')) {   // %variable (an unset one is empty)
                int k = i + 1; while (k < L && (iswalnum(in[k]) || in[k] == L'_')) k++;
                out += GetVar(in.Mid(i, k - i)); i = k - 1; continue;
            }
            if (c != L'$' || i + 1 >= L) { if (hashPending) { out += L'#'; hashPending = false; } out += c; continue; }
            int j = i + 1; bool dbl = false;
            if (in[j] == L'$' && j + 1 < L) { dbl = true; j++; }
            CString val; bool ok = false; int endIdx = j;
            if (in[j] == L'+' && !dbl) {   // $+ : drop the spaces on both sides
                if (hashPending) { out += L'#'; hashPending = false; }
                while (out.GetLength() > 0 && out[out.GetLength() - 1] == L' ') out.Truncate(out.GetLength() - 1);
                int e = j + 1; while (e < L && in[e] == L' ') e++;
                i = e - 1; continue;
            }
            else if (in[j] == L'!' && !dbl) { val = m_lastPrompt; ok = true; endIdx = j + 1; }
            else if (in[j] == L'?') {   // $?  $?="Prompt text"  $?1  (a number: use that parameter if it was given, otherwise ask)
                int k = j + 1, pnum = 0; bool hasN = false;
                while (k < L && iswdigit(in[k])) { if (pnum < 100000) pnum = pnum * 10 + (in[k] - L'0'); hasN = true; k++; }
                CString label = L"Enter a value:";
                if (k + 1 < L && in[k] == L'=' && in[k + 1] == L'"') { int q = in.Find(L'"', k + 2); if (q >= 0) { label = in.Mid(k + 2, q - k - 2); k = q + 1; } }
                if (hasN && pnum >= 1 && pnum <= (int)ts.size()) val = slice(pnum, pnum);
                else {
                    CString typed; CPromptDlg dlg(typed, L"Input", label, this);
                    if (dlg.DoModal() == IDOK) { val = typed; m_lastPrompt = typed; } else m_halt = true;   // Cancel stops the script
                }
                ok = true; endIdx = k;
            }
            else if (iswdigit(in[j])) {   // $0  $N  $N-  $N-M
                int a = 0; while (j < L && iswdigit(in[j])) { if (a < 100000) a = a * 10 + (in[j] - L'0'); j++; }
                if (a == 0) val.Format(L"%d", (int)ts.size());
                else if (j < L && in[j] == L'-') {
                    j++;
                    if (j < L && iswdigit(in[j])) { int b = 0; while (j < L && iswdigit(in[j])) { if (b < 100000) b = b * 10 + (in[j] - L'0'); j++; } val = slice(a, b); }
                    else val = slice(a, -1);
                }
                else val = slice(a, a);
                ok = true; endIdx = j;
            }
            else if (iswalpha(in[j])) {   // named identifier or function; matching is case-insensitive ($ME == $me)
                int k = j; while (k < L && iswalnum(in[k])) k++;
                CString name = in.Mid(j, k - j); name.MakeLower();
                bool done = false;
                // $func(args) -- any identifier can have one, not just $var/aliases (that used to be the only case
                // handled, which silently broke .property access on $window/$line/$sline/$tip and anything else
                // added afterward: a call like $window(1).wid would run $window(1) and then print ".wid" literally,
                // since prop was never populated for a non-var/alias name).
                bool hasArgs = (k < L && in[k] == L'(');
                CString args; int afterArgs = k;
                if (hasArgs) {
                    int depth = 0, m = k;
                    for (; m < L; m++) { if (in[m] == L'(') depth++; else if (in[m] == L')' && --depth == 0) break; }
                    if (m < L) { args = in.Mid(k + 1, m - k - 1); afterArgs = m + 1; } else hasArgs = false;
                }
                // An optional .property, either right after (args) or -- new -- right after a bare identifier with no
                // parentheses at all, e.g. $inwave.fname.
                CString prop; int afterProp = afterArgs;
                if (afterArgs + 1 < L && in[afterArgs] == L'.' && iswalpha(in[afterArgs + 1])) {
                    int pe = afterArgs + 1; while (pe < L && iswalnum(in[pe])) pe++;
                    prop = in.Mid(afterArgs + 1, pe - afterArgs - 1); prop.MakeLower(); afterProp = pe;
                }
                if (hasArgs && FuncValue(w, name, args, prop, params, val)) { done = true; endIdx = afterProp; }
                if (!done) {
                    // If (args) were present but FuncValue didn't recognize the name, fall back to the bare
                    // identifier with no property (matches the original behavior: the "(args)" is left as literal
                    // text). Otherwise -- no parens at all -- pass the property straight through.
                    CString identProp = hasArgs ? CString() : prop;
                    if (IdentValue(w, name, identProp, val)) { done = true; endIdx = hasArgs ? k : afterProp; }
                }
                ok = done;
            }
            if (ok) {
                if (dbl && val.IsEmpty()) m_halt = true;   // $$1, $$?, $$name: no value means "don't run this command"
                if (hashPending) { hashPending = false; if (val.IsEmpty() || !wcschr(L"#&+!", val[0])) out += L'#'; }
                out += val; i = endIdx - 1; continue;
            }
            if (hashPending) { out += L'#'; hashPending = false; }
            out += c;   // unknown identifier or a lone '$': left exactly as typed, so typos are visible
        }
        if (hashPending) out += L'#';
        return out;
    }
    // Like EvalIds, but for a whole command line: the operands that are variable NAMES (of /set /inc /dec /unset /var and of
    // "%x = ...") stay literal, because evaluating them would swap the name for its current value.
    CString EvalCmdLine(CChatWnd* w, const CString& line, const CString& params) {
        CString rest = line, cmd = Word(rest), lc = cmd; lc.MakeLower();
        auto switches = [&](CString& head) {
            for (;;) { CString t = rest; t.TrimLeft(); if (t.Left(1) != L"-" || t.GetLength() < 2) break; head += L" " + Word(rest); }
        };
        if (lc == L"set" || lc == L"inc" || lc == L"dec") {
            CString head = cmd; switches(head);
            CString name = Word(rest);
            return head + (name.IsEmpty() ? CString() : L" " + name) + (rest.IsEmpty() ? CString() : L" " + EvalIds(w, rest, params));
        }
        if (lc == L"unset") return cmd + (rest.IsEmpty() ? CString() : L" " + EvalIds(w, rest, params, false));
        if (lc == L"var") {
            CString res = cmd; switches(res);
            bool first = true; int pos = 0;
            for (CString item = rest.Tokenize(L",", pos); !item.IsEmpty(); item = rest.Tokenize(L",", pos)) {
                item.TrimLeft();
                int k = 0; while (k < item.GetLength() && item[k] != L' ' && item[k] != L'=') k++;
                res += (first ? L" " : L", ") + item.Left(k) + EvalIds(w, item.Mid(k), params);
                first = false;
            }
            return res;
        }
        if (!lc.IsEmpty() && lc[0] == L'%') {   // "%x = 5 + 1": the name stays as typed, the value is evaluated
            int eq = cmd.Find(L'=');
            CString name = eq > 0 ? cmd.Left(eq) : cmd;
            CString tail = eq > 0 ? cmd.Mid(eq) + (rest.IsEmpty() ? CString() : L" " + rest) : rest;
            return name + (tail.IsEmpty() ? CString() : L" " + EvalIds(w, tail, params));
        }
        return EvalIds(w, line, params);
    }
    // ---- aliases: aliases.ini  ([aliases]  n0=/name body ;  a multi-line alias is "/name {", its lines, "}") ----
    AliasDef* FindAlias(const CString& name) { for (size_t i = 0; i < m_aliases.size(); i++) if (m_aliases[i].name.CompareNoCase(name) == 0) return &m_aliases[i]; return nullptr; }
    bool OnRunStack(const CString& name) { for (size_t i = 0; i < m_runStack.size(); i++) if (m_runStack[i].CompareNoCase(name) == 0) return true; return false; }
    void SetAlias(const CString& name, const std::vector<CString>& lines) {
        if (AliasDef* a = FindAlias(name)) a->lines = lines;
        else { AliasDef n; n.name = name; n.lines = lines; m_aliases.push_back(n); }
    }
    std::vector<CString> AliasLines() {   // the file / editor text
        std::vector<CString> out;
        for (size_t i = 0; i < m_aliases.size(); i++) {
            const AliasDef& a = m_aliases[i];
            if (a.lines.size() <= 1) out.push_back(L"/" + a.name + L" " + (a.lines.empty() ? CString() : a.lines[0]));
            else { out.push_back(L"/" + a.name + L" {"); for (size_t k = 0; k < a.lines.size(); k++) out.push_back(L" " + a.lines[k]); out.push_back(CString(L"}")); }
        }
        return out;
    }
    void SaveAliases() {
        CString path = IniPath(L"aliases.ini");
        WritePrivateProfileStringW(L"aliases", nullptr, nullptr, path);   // drop the section, then rewrite it in order
        std::vector<CString> lines = AliasLines();
        for (size_t i = 0; i < lines.size(); i++) { CString key; key.Format(L"n%d", (int)i); WritePrivateProfileStringW(L"aliases", key, lines[i], path); }
    }
    void LoadAliases() {
        CString path = IniPath(L"aliases.ini");
        m_aliases.clear();
        if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
            std::vector<wchar_t> buf(262144, 0);
            DWORD n = GetPrivateProfileSectionW(L"aliases", buf.data(), (DWORD)buf.size(), path);
            std::vector<CString> lines;
            if (n) for (wchar_t* p = buf.data(); *p; p += wcslen(p) + 1) { CString line = p; int eq = line.Find(L'='); if (eq > 0) lines.push_back(line.Mid(eq + 1)); }
            m_aliases = ParseAliases(lines);
            return;
        }
        // first run with aliases.ini: carry over aliases from the old [aliases] section of the main ini, or start with a small default set
        std::vector<wchar_t> buf(65536, 0);
        DWORD n = GetPrivateProfileSectionW(L"aliases", buf.data(), (DWORD)buf.size(), AfxGetApp()->m_pszProfileName);
        if (n) {
            for (wchar_t* p = buf.data(); *p; p += wcslen(p) + 1) {
                CString line = p; int eq = line.Find(L'='); if (eq <= 0) continue;
                std::vector<CString> body; body.push_back(line.Mid(eq + 1)); SetAlias(line.Left(eq), body);
            }
            WritePrivateProfileStringW(L"aliases", nullptr, nullptr, AfxGetApp()->m_pszProfileName);   // moved: drop the old section
        } else {
            static const wchar_t* defs[] = { L"/op /mode # +ooo $$1 $2 $3", L"/dop /mode # -ooo $$1 $2 $3", L"/j /join #$$1 $2-", L"/p /part #",
                L"/n /names #$$1", L"/w /whois $$1", L"/k /kick # $$1 $2-", L"/q /query $$1", L"/send /dcc send $1 $2", L"/chat /dcc chat $1",
                L"/ping /ctcp $$1 ping", L"/s /server $$1-" };
            std::vector<CString> lines; for (size_t i = 0; i < sizeof defs / sizeof defs[0]; i++) lines.push_back(defs[i]);
            m_aliases = ParseAliases(lines);
        }
        SaveAliases();
    }
    afx_msg void OnAliasEditor() {
        CString text; std::vector<CString> cur = AliasLines();
        for (size_t i = 0; i < cur.size(); i++) text += cur[i] + L"\r\n";
        CAliasDlg dlg(text, this);
        if (dlg.DoModal() != IDOK) return;
        std::vector<CString> lines; int pos = 0;
        for (CString piece = text.Tokenize(L"\n", pos); !piece.IsEmpty(); piece = text.Tokenize(L"\n", pos)) { piece.TrimRight(L'\r'); lines.push_back(piece); }
        m_aliases = ParseAliases(lines); SaveAliases();
    }
    afx_msg void OnColorsDialog() {
        CColorsDlg dlg(m_schemes, m_curScheme, this);
        if (dlg.DoModal() != IDOK) return;
        ApplyColorScheme(dlg.Active());
    }
    afx_msg void OnLoggingDialog() {
        CLoggingDlg dlg(m_logEnabled, m_logFolder, this);
        if (dlg.DoModal() != IDOK) return;
        m_logEnabled = dlg.enabled; m_logFolder = dlg.folder;
        if (m_logEnabled && !m_logFolder.IsEmpty()) SHCreateDirectoryExW(nullptr, m_logFolder, nullptr);   // create it (and any missing parent folders) up front
        SaveLogging();
    }

    // ---- the script interpreter: runs an alias body (or a //line) ----
    struct ExecCtx { CChatWnd* w = nullptr; const CString* params = nullptr; CString gotoLabel; };
    enum { C_NEXT = 0, C_BREAK, C_CONTINUE, C_RETURN, C_HALT, C_GOTO };

    // "[ ... ]" evaluation brackets (a space after [ and before ]): the innermost are worked out first and replaced by their value,
    // so  %a [ $+ b ]  becomes  %ab  and  [ [ %x ] ]  evaluates twice.
    CString ExpandBrackets(CChatWnd* w, CString s, const CString& params) {
        for (int guard = 0; guard < 64; guard++) {
            int open = -1;
            for (int i = 0; i + 1 < s.GetLength(); i++) if (s[i] == L'[' && s[i + 1] == L' ' && (i == 0 || s[i - 1] == L' ')) open = i;
            if (open < 0) break;
            int close = -1;
            for (int i = open + 2; i < s.GetLength(); i++) if (s[i] == L']' && s[i - 1] == L' ' && (i + 1 >= s.GetLength() || s[i + 1] == L' ')) { close = i; break; }
            if (close < 0) break;
            int clen = close - open - 3; if (clen < 0) clen = 0;
            CString content = s.Mid(open + 2, clen), before = s.Left(open), after = s.Mid(close + 1);
            content.Trim();
            bool jl = false, jr = false;
            if (content.Left(2) == L"$+" && (content.GetLength() == 2 || content[2] == L' ')) { jl = true; content = content.Mid(2); content.TrimLeft(); }
            if (content.Right(2) == L"$+" && (content.GetLength() == 2 || content[content.GetLength() - 3] == L' ')) { jr = true; content = content.Left(content.GetLength() - 2); content.TrimRight(); }
            CString res = EvalIds(w, content, params);
            if (jl) before.TrimRight();
            if (jr) after.TrimLeft();
            s = before + res + after;
        }
        return s;
    }
    static int FindTop(const CString& s, const wchar_t* op) {   // first occurrence of op outside parentheses, or -1
        int d = 0, n = (int)wcslen(op);
        for (int i = 0; i + n <= s.GetLength(); i++) {
            if (s[i] == L'(') d++; else if (s[i] == L')' && d > 0) d--;
            else if (d == 0 && s.Mid(i, n) == op) return i;
        }
        return -1;
    }
    // if / while conditions:  a == b  a === b  a != b  a < b  a > b  a <= b  a >= b  a isnum [lo-hi]  a isin b  a isincs b
    // a iswm b  a iswmcs b  a ischan   (a leading ! negates the word ones), combined with && and || and parentheses.
    bool EvalCond(CChatWnd* w, const CString& raw, const CString& params) {
        CString s = raw; s.Trim();
        while (s.GetLength() >= 2 && s[0] == L'(' && MatchParen(s, 0) == s.GetLength() - 1) { s = s.Mid(1, s.GetLength() - 2); s.Trim(); }
        int p = FindTop(s, L"||");
        if (p >= 0) return EvalCond(w, s.Left(p), params) || EvalCond(w, s.Mid(p + 2), params);
        p = FindTop(s, L"&&");
        if (p >= 0) return EvalCond(w, s.Left(p), params) && EvalCond(w, s.Mid(p + 2), params);
        CString rest = s.Mid(1); rest.TrimLeft();
        if (s.Left(1) == L"!" && rest.Left(1) == L"(") return !EvalCond(w, rest, params);
        static const wchar_t* symOps[] = { L"===", L"==", L"!=", L"<=", L">=", L"<", L">" };
        static const wchar_t* wordOps[] = { L"isnum", L"isincs", L"isin", L"iswmcs", L"iswm", L"ischan" };
        int pos = -1, len = 0; CString op; int d = 0;
        for (int i = 0; i < s.GetLength() && pos < 0; i++) {
            if (s[i] == L'(') { d++; continue; }
            if (s[i] == L')') { if (d > 0) d--; continue; }
            if (d) continue;
            for (size_t k = 0; k < sizeof symOps / sizeof symOps[0]; k++) { int n = (int)wcslen(symOps[k]); if (s.Mid(i, n) == symOps[k]) { pos = i; len = n; op = symOps[k]; break; } }
            if (pos >= 0) break;
            if (i > 0 && s[i - 1] == L' ') {   // a word operator must stand alone between spaces
                int e = i; while (e < s.GetLength() && s[e] != L' ') e++;
                CString tok = s.Mid(i, e - i), bare = tok; if (bare.Left(1) == L"!") bare = bare.Mid(1);
                for (size_t k = 0; k < sizeof wordOps / sizeof wordOps[0]; k++) if (bare.CompareNoCase(wordOps[k]) == 0) { pos = i; len = e - i; op = tok; break; }
            }
        }
        if (pos < 0) {   // no operator: true if it evaluates to something other than empty / 0
            CString v = EvalIds(w, s, params); v.Trim();
            return !v.IsEmpty() && v != L"0";
        }
        CString lv = EvalIds(w, s.Left(pos), params), rv = EvalIds(w, s.Mid(pos + len), params);
        lv.Trim(); rv.Trim(); op.MakeLower();
        bool neg = false; if (op.Left(1) == L"!" && op != L"!=") { neg = true; op = op.Mid(1); }
        bool r = false;
        if (op == L"==") r = lv.CompareNoCase(rv) == 0;
        else if (op == L"===") r = lv.Compare(rv) == 0;
        else if (op == L"!=") r = lv.CompareNoCase(rv) != 0;
        else if (op == L"<" || op == L">" || op == L"<=" || op == L">=") {
            double a = 0, b = 0;
            if (ParseNum(lv, a) && ParseNum(rv, b)) { if (op == L"<") r = a < b; else if (op == L">") r = a > b; else if (op == L"<=") r = a <= b; else r = a >= b; }
        }
        else if (op == L"isnum") {
            double a = 0; r = ParseNum(lv, a);
            if (r && !rv.IsEmpty()) { int dash = rv.Find(L'-', 1); double lo = 0, hi = 0; if (dash > 0 && ParseNum(rv.Left(dash), lo) && ParseNum(rv.Mid(dash + 1), hi)) r = a >= lo && a <= hi; }
        }
        else if (op == L"isin") { CString a = lv, b = rv; a.MakeLower(); b.MakeLower(); r = b.Find(a) >= 0; }
        else if (op == L"isincs") r = rv.Find(lv) >= 0;
        else if (op == L"iswm") r = GlobMatch(lv, rv);      // the wildcard pattern is on the left
        else if (op == L"iswmcs") r = GlobMatch(lv, rv, true);
        else if (op == L"ischan") r = !lv.IsEmpty() && wcschr(L"#&+!", lv[0]) != nullptr;
        return neg ? !r : r;
    }
    // One command line of a script: evaluated, then either a control word (return halt break continue goto) or a normal command.
    int ExecCmd(ExecCtx& ctx, CString text) {
        text.Trim();
        if (text.IsEmpty() || text[0] == L';') return C_NEXT;
        while (text.Left(1) == L"/") text = text.Mid(1);   // scripts don't need the slash, but aliases.ini bodies usually have one
        text = ExpandBrackets(ctx.w, text, *ctx.params);
        text = EvalCmdLine(ctx.w, text, *ctx.params);
        if (m_halt) return C_HALT;
        text.TrimRight();
        CString rest = text, first = Word(rest); first.MakeLower(); rest.Trim();
        if (first == L"return") { m_result = rest; return C_RETURN; }
        if (first == L"halt") { m_halt = true; return C_HALT; }
        if (first == L"break") return C_BREAK;
        if (first == L"continue") return C_CONTINUE;
        if (first == L"reseterror") return C_NEXT;
        if (first == L"goto") { ctx.gotoLabel = rest; if (ctx.gotoLabel.Left(1) == L":") ctx.gotoLabel = ctx.gotoLabel.Mid(1); return C_GOTO; }
        Dispatch(ctx.w, L"/" + text);
        return m_halt ? C_HALT : C_NEXT;
    }
    int ExecNodes(const std::vector<SNode>& nodes, size_t start, ExecCtx& ctx) {
        for (size_t i = start; i < nodes.size(); i++) {
            if (m_halt) return C_HALT;
            if (++m_steps > 200000) { Show(ctx.w, L"* Script stopped: too many steps (an endless loop?)", cPart); m_halt = true; return C_HALT; }
            const SNode& n = nodes[i]; int r = C_NEXT;
            if (n.kind == 0) r = ExecCmd(ctx, n.text);
            else if (n.kind == 1) { if (EvalCond(ctx.w, n.text, *ctx.params)) r = ExecNodes(n.a, 0, ctx); else if (!n.b.empty()) r = ExecNodes(n.b, 0, ctx); }
            else if (n.kind == 2) {
                while (!m_halt && EvalCond(ctx.w, n.text, *ctx.params)) {
                    if (++m_steps > 200000) { Show(ctx.w, L"* Script stopped: too many steps (an endless loop?)", cPart); m_halt = true; return C_HALT; }
                    int rr = ExecNodes(n.a, 0, ctx);
                    if (rr == C_BREAK) break;
                    if (rr == C_CONTINUE || rr == C_NEXT) continue;
                    r = rr; break;   // return / halt / goto leave the loop
                }
            }
            if (r == C_GOTO) {   // jump to a label at this level; if it isn't here, let the enclosing block look
                bool found = false;
                for (size_t j = 0; j < nodes.size(); j++) if (nodes[j].kind == 3 && nodes[j].text.CompareNoCase(ctx.gotoLabel) == 0) { i = j; found = true; break; }
                if (found) continue;
                return C_GOTO;
            }
            if (r != C_NEXT) return r;
        }
        return C_NEXT;
    }
    void RunScript(CChatWnd* w, const std::vector<CString>& lines, const CString& params) {
        ScopeGuard scope(this, true);   // its own variable scope: /var locals vanish when the script ends
        std::vector<CString> toks = ScriptTokens(lines);
        size_t pos = 0; std::vector<SNode> nodes = ParseNodes(toks, pos, false);
        ExecCtx ctx; ctx.w = w; ctx.params = &params;
        if (ExecNodes(nodes, 0, ctx) == C_GOTO) Show(w, L"* Label not found: " + ctx.gotoLabel, cPart);
    }
    void RunAlias(CChatWnd* w, AliasDef ad, CString params, CString prop = CString()) {   // by value: the body may redefine aliases while it runs
        if (m_runStack.size() >= 24) { Show(w, L"* Aliases nested too deeply (/" + ad.name + L")", cPart); m_halt = true; return; }
        CString savedProp = m_prop; m_prop = prop; m_runStack.push_back(ad.name);
        RunScript(w, ad.lines, params);
        m_runStack.pop_back(); m_prop = savedProp;
    }

    // ---- popup menus: popups.ini  ([mpopup] status  [cpopup] channel  [qpopup] query  [lpopup] nick list  [bpopup] menu bar) ----
    std::vector<CString> m_popRaw[5];                          // each section's lines, exactly as in the file / editor
    std::vector<std::vector<CString>> m_bpActs, m_ctxActs;     // what each menu-bar item / each item of the popup being shown runs
    int m_bpCount = 0; CString m_menuType;                     // how many menu-bar menus were inserted; the value of $menu

    void SeedPopups() {   // used when popups.ini doesn't exist yet
        static const wchar_t* mp[] = { L"Server", L".Lusers:/lusers", L".Motd:/motd", L".Time:/time", L"Names", L".#chan:/names #chan", L".#irchelp: /names #irchelp",
            L".names ?:/names #$$?=\"Enter a channel name:\"", L"Join", L".#chan:/join #chan", L".#irchelp:/join #irchelp", L".join ?:/join #$$?=\"Enter a channel to join:\"",
            L"Query", L".query ?:/query $$?=\"Enter nickname to talk to:\"", L"Other", L".Whois ?:/whois $$?=\"Enter a nickname:\"", L".Query:/query $$?=\"Enter a nickname:\"",
            L".Nickname:/nick $$?=\"Enter your new nickname:\"", L".Away", L"..Set Away...:/away $$?=\"Enter your away message:\"", L"..Set Back:/away", L".List Channels:/list",
            L"-", L"Edit Notes:/run notepad.exe notes.txt", L"Quit IRC:/quit Leaving" };
        static const wchar_t* cp[] = { L"Channel Modes:/channel" };
        static const wchar_t* qp[] = { L"Info:/uwho $$1", L"Whois:/whois $$1", L"Query:/query $$1", L"-", L"Ignore:/ignore $$1 1 | /closemsg $$1", L"-", L"CTCP",
            L".Ping:/ctcp $$1 ping", L".Time:/ctcp $$1 time", L".Version:/ctcp $$1 version", L"DCC", L".Send:/dcc send $$1", L".Chat:/dcc chat $$1" };
        static const wchar_t* lp[] = { L"Info:/uwho $1", L"Whois:/whois $$1", L"Query:/query $$1", L"-", L"Control", L".Ignore:/ignore $$1 1", L".Unignore:/ignore -r $$1 1",            
            L".Op:/mode # +ooo $$1 $2 $3", 
            L".Deop:/mode # -ooo $$1 $2 $3", 
            L".Halfop:/mode # +hhh $$1 $2 $3",
            L".DeHalfop:/mode # -hhh $$1 $2 $3",
            L".Voice:/mode # +vvv $$1 $2 $3", 
            L".Devoice:/mode # -vvv $$1 $2 $3", 
            L".Kick:/kick # $$1",
            L".Kick (why):/kick # $$1 $$?=\"Reason:\"", L".Ban:/ban $$1 2", L".Ban, Kick:/ban $$1 2 | /timer 1 3 /kick # $$1", L".Ban, Kick (why):/ban $$1 2 | /timer 1 3 /kick # $$1 $$?=\"Reason:\"",
            L"CTCP", L".Ping:/ctcp $$1 ping", L".Time:/ctcp $$1 time", L".Version:/ctcp $$1 version", L"DCC", L".Send:/dcc send $$1", L".Chat:/dcc chat $$1", L"-",
            L"Slap!:/me slaps $$1 around a bit with a large trout" };
        static const wchar_t* bp[] = { L"Commands", L"Join channel:/join #$$?=\"Enter channel name:\"", L"Part channel:/part #$$?=\"Enter channel name:\"",
            L"Query user:/query $$?=\"Enter nickname and message:\"", L"Send notice:/notice $$?=\"Enter nickname and message:\"", L"Whois user:/whois $$?=\"Enter nickname:\"",
            L"Send CTCP", L".Ping:/ctcp $$?=\"Enter nickname:\" ping", L".Time:/ctcp $$?=\"Enter nickname:\" time", L".Version:/ctcp $$?=\"Enter nickname:\" version",
            L"Set Away", L".On:/away $$?=\"Enter away message:\"", L".Off:/away", L"Invite user:/invite $$?=\"Enter nickname and channel:\"",
            L"Ban user:/ban $$?=\"Enter channel and nickname:\"", L"Kick user:/kick $$?=\"Enter channel and nickname:\"", L"Ignore user:/ignore $$?=\"Enter nickname:\"",
            L"Unignore user:/ignore -r $$?=\"Enter nickname:\"", L"Change nick:/nick $$?=\"Enter new nickname:\"", L"Quit IRC:/quit" };
        auto fill = [&](int sec, const wchar_t* const* a, size_t n) { m_popRaw[sec].clear(); for (size_t i = 0; i < n; i++) m_popRaw[sec].push_back(a[i]); };
        fill(0, mp, sizeof mp / sizeof mp[0]); fill(1, cp, sizeof cp / sizeof cp[0]); fill(2, qp, sizeof qp / sizeof qp[0]);
        fill(3, lp, sizeof lp / sizeof lp[0]); fill(4, bp, sizeof bp / sizeof bp[0]);
    }
    void SavePopups() {
        CString path = IniPath(L"popups.ini");
        for (int s = 0; s < 5; s++) {
            WritePrivateProfileStringW(kPopSec[s], nullptr, nullptr, path);   // drop the section, then rewrite it in order
            for (size_t i = 0; i < m_popRaw[s].size(); i++) { CString key; key.Format(L"n%d", (int)i); WritePrivateProfileStringW(kPopSec[s], key, m_popRaw[s][i], path); }
        }
    }
    void LoadPopups() {
        CString path = IniPath(L"popups.ini");
        for (int s = 0; s < 5; s++) m_popRaw[s].clear();
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) { SeedPopups(); SavePopups(); return; }
        std::vector<wchar_t> buf(262144, 0);
        for (int s = 0; s < 5; s++) {
            DWORD n = GetPrivateProfileSectionW(kPopSec[s], buf.data(), (DWORD)buf.size(), path);
            if (!n) continue;
            for (wchar_t* p = buf.data(); *p; p += wcslen(p) + 1) { CString line = p; int eq = line.Find(L'='); if (eq > 0) m_popRaw[s].push_back(line.Mid(eq + 1)); }
        }
    }
    afx_msg void OnPopupEditor(UINT id) {
        int sec = (int)id - (int)IDM_POPEDIT0; if (sec < 0 || sec > 4) return;
        static const wchar_t* names[5] = { L"Status window popup", L"Channel window popup", L"Query window popup", L"Nick list popup", L"Menu bar popup" };
        CString text; for (size_t i = 0; i < m_popRaw[sec].size(); i++) text += m_popRaw[sec][i] + L"\r\n";
        CAliasDlg dlg(text, this, names[sec], L"One item per line:   Title:/command     .Sub item     -  (separator)     Title {  lines  }");
        if (dlg.DoModal() != IDOK) return;
        m_popRaw[sec].clear(); int pos = 0;
        for (CString piece = text.Tokenize(L"\n", pos); !piece.IsEmpty(); piece = text.Tokenize(L"\n", pos)) { piece.TrimRight(L'\r'); m_popRaw[sec].push_back(piece); }
        SavePopups();
        if (sec == 4) RebuildMenuBarPopups();
    }
    CChatWnd* ActiveOrStatus() {
        CChatWnd* a = dynamic_cast<CChatWnd*>(MDIGetActive());
        if (a) return a;
        if (!m_nets.empty()) return Status(m_nets.front().get());
        return nullptr;
    }
    void RunPopupLines(CChatWnd* w, const std::vector<CString>& lines, const CString& params) {   // a chosen item: its commands run like an alias body, with $1.. = params
        if (!w) return;
        if (m_depth == 0) { m_halt = false; m_steps = 0; }
        m_depth++; RunScript(w, lines, params); m_depth--;
    }
    // $submenu($id($1)) items are replaced by the one-line menu items the identifier returns (called with begin, 1, 2, ... and end).
    std::vector<PopupItem> ExpandSubmenus(const std::vector<PopupItem>& in, CChatWnd* w, const CString& params) {
        std::vector<PopupItem> out;
        for (size_t k = 0; k < in.size(); k++) {
            const PopupItem& it = in[k];
            CString t = it.title; t.Trim();
            if (t.Left(9).CompareNoCase(L"$submenu(") != 0 || t.Right(1) != L")") { out.push_back(it); continue; }
            if (!w) continue;
            CString inner = t.Mid(9, t.GetLength() - 10);
            auto ask = [&](const CString& p) { CString r = EvalIds(w, inner, p); m_halt = false; r.Trim(); return r; };
            CString begin = ask(CString(L"begin"));
            std::vector<CString> got;
            for (int n = 1; n <= 300; n++) { CString nn; nn.Format(L"%d", n); CString r = ask(nn); if (r.IsEmpty()) break; got.push_back(r); }
            CString end = ask(CString(L"end"));
            if (got.empty()) continue;
            CString dots(L'.', it.depth);
            if (begin == L"-") { PopupItem sp; sp.title = L"-"; sp.depth = it.depth; out.push_back(sp); }
            for (size_t g = 0; g < got.size(); g++) {
                std::vector<CString> one; one.push_back(dots + got[g]);
                std::vector<PopupItem> parsed = ParsePopupItems(one);
                for (size_t q = 0; q < parsed.size(); q++) out.push_back(parsed[q]);
            }
            if (end == L"-") { PopupItem sp; sp.title = L"-"; sp.depth = it.depth; out.push_back(sp); }
        }
        return out;
    }
    void SkipKids(const std::vector<PopupItem>& items, size_t& i, int depth) { while (i < items.size() && items[i].depth > depth) i++; }
    // Builds the items at 'depth' (starting at items[i]) into m. Titles are evaluated now, each time, so they can vary ($iif, $style, $1...);
    // a title that comes out empty hides the item. Ids are handed out from 'base' and what each one runs is recorded in acts.
    void BuildPopupLevel(CMenu& m, const std::vector<PopupItem>& items, size_t& i, int depth, UINT base, std::vector<std::vector<CString>>& acts, CChatWnd* w, const CString& params) {
        bool lastSep = true;   // no separator first, none twice in a row, none last
        while (i < items.size()) {
            const PopupItem& it = items[i];
            if (it.depth < depth) break;
            if (it.depth > depth) { i++; continue; }
            i++;
            bool hasKids = i < items.size() && items[i].depth > depth;
            CString t = EvalIds(w, it.title, params); m_halt = false; t.Trim();
            UINT flags = 0;
            while (t.GetLength() >= 3 && t[0] == 0x1E && t[2] == 0x1E) { int n = t[1] - L'0'; if (n & 1) flags |= MF_CHECKED; if (n & 2) flags |= MF_GRAYED; t = t.Mid(3); t.TrimLeft(); }
            if (t.IsEmpty()) { SkipKids(items, i, depth); continue; }
            if (t == L"-" && it.cmd.empty()) { if (!lastSep) { m.AppendMenu(MF_SEPARATOR); lastSep = true; } SkipKids(items, i, depth); continue; }
            if (hasKids) {
                CMenu sub; sub.CreatePopupMenu();
                BuildPopupLevel(sub, items, i, depth + 1, base, acts, w, params);
                if (sub.GetMenuItemCount() > 0) { m.AppendMenu(MF_POPUP | flags, (UINT_PTR)sub.Detach(), t); lastSep = false; }
            } else if (!it.cmd.empty() && acts.size() < 900) {
                m.AppendMenu(MF_STRING | flags, base + (UINT)acts.size(), t); acts.push_back(it.cmd); lastSep = false;
            } else { m.AppendMenu(MF_STRING | MF_GRAYED, 0, t); lastSep = false; }
        }
        int n = m.GetMenuItemCount();
        if (lastSep && n > 0) m.DeleteMenu(n - 1, MF_BYPOSITION);
    }
    // Shows popup section 'sec' for window w at pt; params are $1 $2 ... (the selected nicks, or the query's nick). Returns false if that
    // popup has nothing to show, so the caller can fall back to the default menu.
    bool ShowContextPopup(CChatWnd* w, int sec, const CString& params, CPoint pt, bool canCopy) {
        if (!w) return false;
        if (m_popRaw[sec].empty() && !canCopy) return false;
        if (m_depth == 0) { m_halt = false; m_steps = 0; }
        m_depth++;
        m_menuType = kPopType[sec];
        std::vector<PopupItem> items = m_popRaw[sec].empty() ? std::vector<PopupItem>() : ExpandSubmenus(ParsePopupItems(m_popRaw[sec]), w, params);
        CMenu m; m.CreatePopupMenu(); m_ctxActs.clear();
        if (canCopy) { m.AppendMenu(MF_STRING, IDP_COPY, L"Copy"); m.AppendMenu(MF_SEPARATOR); }
        size_t i = 0;
        if (!items.empty()) BuildPopupLevel(m, items, i, 0, IDP_CTX, m_ctxActs, w, params);
        m_depth--;
        if (m.GetMenuItemCount() == 0) return false;
        SetForegroundWindow(); m_menuOpen = true;
        int cmd = m.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
        m_menuOpen = false; PostMessage(WM_NULL, 0, 0);
        if (cmd == IDP_COPY) w->LogCopy();
        else if (cmd >= IDP_CTX && (size_t)(cmd - IDP_CTX) < m_ctxActs.size()) { std::vector<CString> lines = m_ctxActs[cmd - IDP_CTX]; RunPopupLines(w, lines, params); }
        return true;
    }
    bool ShowWindowPopup(CChatWnd* c, CPoint pt) {   // right-click in a chat log: status / channel / query popup
        int sec = c->m_name == L"*status*" ? 0 : (c->m_chan ? 1 : 2);
        CString params; if (sec == 2) params = c->m_name;   // in a query window $1 is the person you're talking to
        return ShowContextPopup(c, sec, params, pt, c->LogHasSelection());
    }
    bool ShowCustomPopup(CChatWnd* w, CPoint pt) {   // right-click in an @window: its own popup.txt, loaded fresh each time
        if (w->m_cwPopup.empty()) return false;
        std::vector<PopupItem> items = ParsePopupItems(w->m_cwPopup);
        CMenu m; m.CreatePopupMenu(); std::vector<std::vector<CString>> acts; size_t i = 0;
        BuildPopupLevel(m, items, i, 0, IDP_CTX, acts, w, CString());
        if (m.GetMenuItemCount() == 0) return false;
        SetForegroundWindow(); m_menuOpen = true;
        int cmd = m.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
        m_menuOpen = false; PostMessage(WM_NULL, 0, 0);
        if (cmd >= IDP_CTX && (size_t)(cmd - IDP_CTX) < acts.size()) RunPopupLines(w, acts[cmd - IDP_CTX], CString());
        return true;
    }
    void ShowNickMenu(CChatWnd* c, const CString& nicks, CPoint pt) {   // right-click nick(s) in the user list: [lpopup], or the built-in menu if it's empty
        if (ShowContextPopup(c, 3, nicks, pt, false)) return;
        CString rest = nicks, first = Word(rest);
        ShowNickMenuLegacy(c, first, pt);
    }
    int FindMenuBarIndex(const wchar_t* name) {
        int n = m_menu.GetMenuItemCount();
        for (int i = 0; i < n; i++) { CString s; m_menu.GetMenuString(i, s, MF_BYPOSITION); s.Remove(L'&'); if (s.CompareNoCase(name) == 0) return i; }
        return -1;
    }
    // [bpopup]: a bare top-level heading (no command, no sub items) names a menu-bar menu that takes the top-level items after it;
    // with no such heading, each top-level item that has sub items becomes a menu of its own.
    void RebuildMenuBarPopups() {
        if (!m_menu.GetSafeHmenu()) return;
        int win = FindMenuBarIndex(L"Window"); if (win < 0) return;
        for (int k = 0; k < m_bpCount && win - m_bpCount >= 0; k++) m_menu.DeleteMenu(win - m_bpCount, MF_BYPOSITION);
        m_bpCount = 0; m_bpActs.clear();
        win = FindMenuBarIndex(L"Window"); if (win < 0) return;
        std::vector<PopupItem> items = ParsePopupItems(m_popRaw[4]);
        struct Grp { CString title; std::vector<PopupItem> items; };
        std::vector<Grp> groups; int cur = -1;
        for (size_t i = 0; i < items.size();) {
            size_t j = i + 1; while (j < items.size() && items[j].depth > items[i].depth) j++;   // this item and its sub items are [i, j)
            const PopupItem& it = items[i];
            if (it.depth != 0) { i = j; continue; }
            bool bare = (j == i + 1) && it.cmd.empty() && it.title != L"-";
            if (bare) { Grp g; g.title = it.title; groups.push_back(g); cur = (int)groups.size() - 1; }
            else if (cur >= 0) { for (size_t k = i; k < j; k++) groups[cur].items.push_back(items[k]); }
            else if (j > i + 1) { Grp g; g.title = it.title; for (size_t k = i + 1; k < j; k++) { PopupItem c2 = items[k]; c2.depth--; g.items.push_back(c2); } groups.push_back(g); }
            else { Grp g; g.title = L"Popups"; g.items.push_back(it); groups.push_back(g); cur = (int)groups.size() - 1; }
            i = j;
        }
        m_menuType = L"menubar"; int pos = win;
        for (size_t g = 0; g < groups.size(); g++) {
            if (groups[g].items.empty()) continue;
            CMenu sub; sub.CreatePopupMenu(); size_t i = 0;
            BuildPopupLevel(sub, groups[g].items, i, 0, IDP_BAR, m_bpActs, nullptr, CString());
            if (sub.GetMenuItemCount() == 0) continue;
            CString title = EvalIds(nullptr, groups[g].title, CString()); m_halt = false; title.Trim();
            if (title.IsEmpty()) title = groups[g].title;
            m_menu.InsertMenu(pos++, MF_BYPOSITION | MF_POPUP, (UINT_PTR)sub.Detach(), title);
            m_bpCount++;
        }
        DrawMenuBar();
    }
    afx_msg void OnMenubarPopup(UINT id) {   // an item of a menu-bar popup: runs in the window you're in
        size_t idx = (size_t)id - IDP_BAR; if (idx >= m_bpActs.size()) return;
        std::vector<CString> lines = m_bpActs[idx];
        CChatWnd* w = ActiveOrStatus();
        if (w) RunPopupLines(w, lines, CString());
    }

    // ---- /channel: Channel Central ----
    CChanCentralDlg* m_cc = nullptr;   // the dialog while it's open, so the server's replies can be routed to it
    void OpenChannelCentral(CChatWnd* w, CString arg) {
        Net* net = w->net; if (!net) return;
        CString a = arg, chan = Word(a);
        if (chan.IsEmpty() && w->m_chan) chan = w->m_name;
        if (chan.IsEmpty() || !wcschr(L"#&+!", chan[0])) { Show(w, L"* Usage: /channel [#channel]   (or use it in a channel window)", cPart); return; }
        if (!net->conn) { Show(w, L"* Not connected.", cPart); return; }
        if (m_cc) return;   // one at a time
        CChanCentralDlg dlg(chan, this);
        dlg.net = net; dlg.chanModes = net->chanmodes;
        if (CChatWnd* cw = Find(net, chan)) { dlg.curTopic = cw->m_topicRaw; dlg.topicHist = cw->m_topicHist; }
        dlg.sendRaw = [this, net](const CString& l) { Send(net, l); };
        m_cc = &dlg;
        dlg.DoModal();   // its constructor-side setup asks the server for the modes and ban list; the replies arrive through HandleCCNumeric
        m_cc = nullptr;
    }
    bool HandleCCNumeric(Net* net, const CString& cmd, const std::vector<CString>& p) {
        if (!m_cc || m_cc->net != net) return false;
        auto P = [&](size_t i) { return i < p.size() ? p[i] : CString(); };
        if (P(1).CompareNoCase(m_cc->chan) != 0) return false;   // every reply we care about names the channel as its first parameter after our nick
        if (cmd == L"324") { std::vector<CString> args; for (size_t i = 3; i < p.size(); i++) args.push_back(p[i]); m_cc->SetModes(P(2), args); return true; }   // RPL_CHANNELMODEIS
        if (cmd == L"329") return true;                                                                                                  // channel creation time: not shown
        if (cmd == L"367") { m_cc->AddEntry(0, P(2), P(3), P(4)); return true; }   if (cmd == L"368") { m_cc->EndList(0); return true; }   // bans
        if (cmd == L"348") { m_cc->AddEntry(1, P(2), P(3), P(4)); return true; }   if (cmd == L"349") { m_cc->EndList(1); return true; }   // excepts
        if (cmd == L"346") { m_cc->AddEntry(2, P(2), P(3), P(4)); return true; }   if (cmd == L"347") { m_cc->EndList(2); return true; }   // invites
        if (cmd == L"728") { m_cc->AddEntry(3, P(3), P(4), P(5)); return true; }   if (cmd == L"729") { m_cc->EndList(3); return true; }   // quiets (charybdis-style servers)
        if (cmd == L"482") { m_cc->Status(P(2)); return true; }                                                                          // "You're not channel operator"
        return false;
    }

    // ---- user input ----
    void OnInput(CChatWnd* w, CString s) {   // whatever was typed (or issued from a menu)
        if (s.IsEmpty()) return;
        if (m_depth == 0) { m_halt = false; m_steps = 0; }
        m_depth++;
        if (s.Left(2) == L"//") { std::vector<CString> one; one.push_back(s.Mid(2)); RunScript(w, one, CString()); }   // "//cmd | cmd": a one-line script, identifiers evaluated
        else { ScopeGuard scope(this, true); Dispatch(w, s); }
        m_depth--;
    }
    void Dispatch(CChatWnd* w, CString s) {   // one finished line: plain chat text, or "/command args"
        Net* net = w->net;
        if (s.IsEmpty()) return;
        if (s[0] != L'/') {
            if (w->m_custom) { if (!w->m_cwDefCmd.IsEmpty()) RunScript(w, std::vector<CString>{ w->m_cwDefCmd }, s); return; }
            if (w->m_name == L"*status*") Note(net, L"You're not in a channel or query.", cPart);
            else Say(net, w->m_name, s);
            return;
        }
        CString arg = s.Mid(1), cmdRaw = Word(arg);
        bool bypass = false;
        if (cmdRaw.Left(1) == L"!") { bypass = true; cmdRaw = cmdRaw.Mid(1); }   // "/!join": skip any alias of that name
        else if (cmdRaw.Left(1) == L".") cmdRaw = cmdRaw.Mid(1);                 // "/.cmd": accepted, but its output isn't suppressed
        CString cmd = cmdRaw; cmd.MakeLower();
        if (cmd.IsEmpty()) return;
        if (cmd[0] == L'%') {   // "%x = value" (also "%x=value"): assignment
            int eq = cmdRaw.Find(L'=');
            if (eq > 0) { arg = cmdRaw.Mid(eq) + (arg.IsEmpty() ? CString() : L" " + arg); cmdRaw = cmdRaw.Left(eq); }
            CmdAssign(w, cmdRaw, arg);
            return;
        }
        bool inChat = w->m_name != L"*status*";
        if (!bypass) {
            AliasDef* ad = FindAlias(cmd);
            if (ad && !OnRunStack(ad->name)) { RunAlias(w, *ad, arg); return; }   // an alias may still call the built-in command of its own name
        }
        if (cmd == L"timers") { CmdTimers(w, arg); return; }   // reserved: "/timers" is always the list/off-all command, never a timer literally named "s"
        if (cmd == L"identd") { CmdIdentd(w, arg); return; }
        if (cmd == L"tray") { CmdTray(w, arg); return; }
        if (cmd == L"tips") { CmdTips(w, arg); return; }
        if (cmd == L"tip") { CmdTip(w, arg); return; }
        if (cmd == L"titlebar") { CmdTitlebar(w, arg); return; }
        if (cmd == L"splay") { CmdSplay(w, arg); return; }
        if (cmd == L"vol") { CmdVol(w, arg); return; }
        if (cmd == L"timer") { CmdTimer(w, CString(), arg); return; }   // bare "/timer": auto-assigns the next free number
        if (cmd.Left(5) == L"timer" && cmd.GetLength() > 5) { CmdTimer(w, cmdRaw.Mid(5), arg); return; }   // "/timer1", "/timershow", etc: the timer name follows directly, no space
        if (cmd == L"server" || cmd == L"connect") {
            bool multi = false; arg.TrimLeft();
            if (arg.Left(2).CompareNoCase(L"-m") == 0) { multi = true; arg = arg.Mid(2); arg.TrimLeft(); }
            CString h = Word(arg); arg.Trim();
            if (h.IsEmpty()) { Note(net, L"Usage: /server [-m] <host> [port | +port for TLS]  (-m: connect as a separate, additional network)", cPart); return; }
            Net* target = multi ? NewNet() : net;                 // -m: brand-new independent network; else replace this window's own network
            bool tls = target->o.tls;
            if (arg.Left(1) == L"+") { tls = true; arg = arg.Mid(1); } else if (!arg.IsEmpty()) tls = false;
            target->o.tls = tls; target->o.host = h; target->o.port = _ttoi(arg) ? _ttoi(arg) : (tls ? 6697 : 6667);
            target->tag = h; m_defOpts = target->o; SaveOpts();
            Status(target); if (multi) Activate(Status(target));
            Connect(target, h, target->o.port);
        }
        else if (cmd == L"nick") { if (net->conn) Send(net, L"NICK " + arg); else net->nick = arg; }
        else if (cmd == L"join" || cmd == L"j") Send(net, L"JOIN " + arg);
        else if (cmd == L"list") {
            CString a2 = arg; bool minimize = false; CString minS, maxS, pattern;
            for (;;) {
                CString tok = Word(a2); if (tok.IsEmpty()) break;
                CString up = tok; up.MakeUpper();
                if (up == L"-N") minimize = true;
                else if (up == L"-MIN") minS = Word(a2);
                else if (up == L"-MAX") maxS = Word(a2);
                else pattern = tok;
            }
            CString param;
            if (!minS.IsEmpty() || !maxS.IsEmpty()) { if (!minS.IsEmpty()) param += L">" + minS; if (!maxS.IsEmpty()) param += L"<" + maxS; }
            else if (!pattern.IsEmpty()) param = pattern;
            CListWnd* lw = OpenListWnd(net);
            lw->Clear();
            Send(net, L"LIST" + (param.IsEmpty() ? CString() : L" " + param));
            if (minimize) lw->ShowWindow(SW_SHOWMINIMIZED);
        }
        else if (cmd == L"part" || cmd == L"leave") Send(net, L"PART " + (arg.IsEmpty() && w->m_chan ? w->m_name : arg));
        else if (cmd == L"msg" || cmd == L"m") { CString t = Word(arg); Say(net, t, arg); }
        else if (cmd == L"query" || cmd == L"q") { CString t = Word(arg); Open(net, t, false); if (!arg.IsEmpty()) Say(net, t, arg); }
        else if (cmd == L"me" && inChat) Say(net, w->m_name, arg, true);
        else if (cmd == L"notice") { CString t = Word(arg); Send(net, L"NOTICE " + t + L" :" + arg); Note(net, L"-> -" + t + L"- " + arg, cNotice); }
        else if (cmd == L"ctcp") {
            CString t = Word(arg); CString type = Word(arg); type.MakeUpper();
            if (t.IsEmpty() || type.IsEmpty()) { Note(net, L"Usage: /ctcp <nick> <version|time|ping> [args]", cPart); return; }
            CString payload = type;
            if (type == L"PING" && arg.IsEmpty()) { CString ts; ts.Format(L"%lu", ::GetTickCount()); payload += L" " + ts; }
            else if (!arg.IsEmpty()) payload += L" " + arg;
            Send(net, L"PRIVMSG " + t + L" :" + CString(wchar_t(1)) + payload + CString(wchar_t(1)));
            Note(net, L"[CTCP " + type + L" to " + t + L"]", cCTCP);
        }
        else if (cmd == L"topic") {   // /topic [#channel] [new topic]: a named channel is optional (defaults to the current one); no topic text means query, not set
            CString a = arg; CString first = Word(a);
            CString chan = IsChan(first) ? first : (w->m_chan ? w->m_name : CString());
            CString topicText = IsChan(first) ? a : arg;
            if (chan.IsEmpty()) { Show(w, L"* Usage: /topic [#channel] [new topic]", cTopic); return; }
            Send(net, topicText.IsEmpty() ? L"TOPIC " + chan : L"TOPIC " + chan + L" :" + topicText);
        }
        else if (cmd == L"quit") { Send(net, L"QUIT :" + (arg.IsEmpty() ? CString(VERSION) : arg)); net->conn = false; net->sock.Close(); SetState(net, L"Disconnected"); }
        else if (cmd == L"clear") w->Clear();
        else if (cmd == L"echo") {   // /echo [color] [-switches] [-c color name] [#channel|nick] <text>  (local only: never sent to the server)
            CString a = arg; a.Trim();
            int colorNum = -1;
            { CString tmp = a; CString first = Word(tmp); if (!first.IsEmpty() && IsAllDigits(first)) { colorNum = _wtoi(first); a = tmp; } }
            bool eFlag = false, hFlag = false, tFlag = false, sFlag = false, aFlagSw = false, qFlag = false, lFlag = false,
                 bFlag = false, fFlag = false, nFlag = false, gFlag = false, cFlag = false;
            int indentN = 0;
            if (a.Left(1) == L"-") {
                int i = 1; for (; i < a.GetLength() && a[i] != L' '; i++) {
                    wchar_t c = a[i];
                    if (c == L'c') cFlag = true; else if (c == L'e') eFlag = true; else if (c == L'g') gFlag = true;
                    else if (c == L'h') hFlag = true; else if (c == L's') sFlag = true; else if (c == L'a') aFlagSw = true;
                    else if (c == L'q') qFlag = true; else if (c == L'l') lFlag = true; else if (c == L'b') bFlag = true;
                    else if (c == L'f') fFlag = true; else if (c == L'n') nFlag = true; else if (c == L't') tFlag = true;
                    else if (c == L'i') { CString digs; while (i + 1 < a.GetLength() && iswdigit(a[i + 1])) digs += a[++i]; indentN = digs.IsEmpty() ? 0 : _wtoi(digs); }
                    // -d, -m, -r are accepted (so a mIRC-style switch string doesn't error out) but have no distinct
                    // effect here: this client has no separate "single message" window type, message-vs-event
                    // distinction, or strip-settings dialog to apply them to.
                }
                a = a.Mid(i); a.TrimLeft();
            }
            COLORREF col = colorNum >= 0 ? MircColor(colorNum) : cText;
            if (cFlag) {
                CString name = Word(a); CString nl = name; nl.MakeLower();
                if (nl == L"normal") col = cText; else if (nl == L"own") col = cOwn; else if (nl == L"join") col = cJoin;
                else if (nl == L"part") col = cPart; else if (nl == L"notice") col = cNotice; else if (nl == L"info") col = cInfo;
                else if (nl == L"action") col = cAction;
                else { Show(w, L"* /echo: unknown color name '" + name + L"' (try Normal, Own, Join, Part, Notice, Info, Action).", cPart, 0); return; }
            }
            CChatWnd* target = nullptr;
            { CString tmp = a; CString first = Word(tmp);
              if (!first.IsEmpty()) { CString tn = first; if (tn[0] == L'=') tn = tn.Mid(1);
                  CChatWnd* found = tn[0] == L'@' ? Find(nullptr, tn) : Find(net, tn);   // @windows are stored keyed under no network, not the current one
                  if (IsChan(tn) || found) { target = IsChan(tn) ? Open(net, tn, true) : found; a = tmp; } } }
            if (!target) target = sFlag ? Status(net) : aFlagSw ? dynamic_cast<CChatWnd*>(MDIGetActive()) : w;
            if (!target) { Show(w, L"* /echo: no such window.", cPart, 0); return; }
            CString text = a;
            if (indentN > 0) { CString pad; pad.Format(L"%*s", indentN, L""); text = pad + text; }
            if (hFlag) {   // hard-wrap at a fixed width, so it doesn't reflow if the window is resized later
                CString wrapped; int col2 = 0;
                for (int i = 0; i < text.GetLength(); i++) { wrapped += text[i]; if (++col2 >= 80 && text[i] == L' ') { wrapped += L"\r\n"; col2 = 0; } }
                text = wrapped;
            }
            if (eFlag) text = L"---- " + text + L" ----";
            // -q: mIRC suppresses output when /echo is invoked via a ".command" quiet prefix from a script. This
            // client doesn't have that quiet-prefix mechanism (see the alias/scripting notes), so -q is accepted
            // for compatibility but doesn't change anything -- there's no "was this quiet-prefixed" state to check.
            int savedAct = target->m_act;
            auto savedLog = target->onLog; if (gFlag) target->onLog = nullptr;
            Show(target, text, col, tFlag ? -1 : 0);
            if (gFlag) target->onLog = savedLog;
            if (nFlag) target->m_act = savedAct;
            if (lFlag) target->m_act = 2;   // -l: apply "highlight" treatment -- the strongest activity level this client has
            if (bFlag) MessageBeep(MB_OK);
            if (fFlag) FlashWindow(TRUE);
        }
        else if (cmd == L"say") { if (inChat) Say(net, w->m_name, arg); else Note(net, L"You're not in a channel or query.", cPart); }   // sends text as-is, even if it starts with a slash
        else if (cmd == L"alias") {   // /alias (list)   /alias name (show)   /alias name commands (define; File > Aliases... edits multi-line ones)
            CString name = Word(arg); arg.Trim();
            if (name.Left(1) == L"/") name = name.Mid(1);
            if (name.IsEmpty()) {
                if (m_aliases.empty()) Show(w, L"* No aliases defined. Usage: /alias <name> <commands>   (File > Aliases... edits them all)", cInfo);
                for (size_t i = 0; i < m_aliases.size(); i++) Show(w, L"* /" + m_aliases[i].name + L" " + (m_aliases[i].lines.size() == 1 ? m_aliases[i].lines[0] : CString(L"{ ... }")), cInfo);
            } else if (arg.IsEmpty()) {
                AliasDef* ad = FindAlias(name);
                if (!ad) Show(w, L"* No such alias: " + name, cInfo);
                else for (size_t i = 0; i < ad->lines.size(); i++) Show(w, L"* /" + ad->name + L": " + ad->lines[i], cInfo);
            } else {
                std::vector<CString> body; body.push_back(arg); SetAlias(name, body); SaveAliases();
                Show(w, L"* Alias /" + name + L" defined", cInfo);
            }
        }
        else if (cmd == L"set") CmdSet(w, arg);
        else if (cmd == L"unset") CmdUnset(w, arg);
        else if (cmd == L"unsetall") { m_vars.clear(); m_varsDirty = true; if (!m_scopes.empty()) m_scopes.back().locals.clear(); FlushVars(); }
        else if (cmd == L"var") CmdVar(w, arg);
        else if (cmd == L"inc") CmdIncDec(w, cmd, arg, 1);
        else if (cmd == L"dec") CmdIncDec(w, cmd, arg, -1);
        else if (cmd == L"unalias") {
            CString name = Word(arg); if (name.Left(1) == L"/") name = name.Mid(1);
            bool gone = false;
            for (size_t i = 0; i < m_aliases.size(); i++) if (m_aliases[i].name.CompareNoCase(name) == 0) { m_aliases.erase(m_aliases.begin() + i); gone = true; break; }
            if (gone) { SaveAliases(); Show(w, L"* Alias /" + name + L" removed", cInfo); }
            else Show(w, L"* No such alias: " + name, cInfo);
        }
        else if (cmd == L"channel") OpenChannelCentral(w, arg);
        else if (cmd == L"colors") OnColorsDialog();
        else if (cmd == L"logging") OnLoggingDialog();
        else if (cmd == L"play") CmdPlay(w, arg);
        else if (cmd == L"playctrl") CmdPlayCtrl(w);
        else if (cmd == L"dns") CmdDns(w, arg);
        else if (cmd == L"window") CmdWindow(w, arg);
        else if (cmd == L"aline") CmdCwLine(w, arg, L'a');
        else if (cmd == L"cline") CmdCwLine(w, arg, L'c');
        else if (cmd == L"dline") CmdCwLine(w, arg, L'd');
        else if (cmd == L"iline") CmdCwLine(w, arg, L'i');
        else if (cmd == L"rline") CmdCwLine(w, arg, L'r');
        else if (cmd == L"sline") CmdCwLine(w, arg, L's');
        else if (cmd == L"renwin") CmdRenwin(w, arg);
        else if (cmd == L"timestamp") {
            CString a = arg; a.Trim();
            if (a.Left(2).CompareNoCase(L"-f") == 0) {
                m_tsEventFmt = a.Mid(2); m_tsEventFmt.Trim(); if (m_tsEventFmt.IsEmpty()) m_tsEventFmt = L"[HH:nn]";
                SaveTimestamp(); Show(w, L"* Event timestamp format: " + m_tsEventFmt, cInfo);
            } else if (a.Left(2).CompareNoCase(L"-g") == 0) {
                m_tsLogFmt = a.Mid(2); m_tsLogFmt.Trim(); if (m_tsLogFmt.IsEmpty()) m_tsLogFmt = L"[HH:nn:ss]";
                SaveTimestamp(); Show(w, L"* Log timestamp format: " + m_tsLogFmt, cInfo);
            } else {
                int target = 0;   // 0 = no target given (global); 1 = status; 2 = active; 3 = every
                if (a.Left(2).CompareNoCase(L"-s") == 0) { target = 1; a = a.Mid(2); a.TrimLeft(); }
                else if (a.Left(2).CompareNoCase(L"-a") == 0) { target = 2; a = a.Mid(2); a.TrimLeft(); }
                else if (a.Left(2).CompareNoCase(L"-e") == 0) { target = 3; a = a.Mid(2); a.TrimLeft(); }
                CString mode = Word(a); CString modeL = mode; modeL.MakeLower();
                int val = modeL == L"on" ? 1 : modeL == L"off" ? 0 : modeL == L"default" ? -1 : -2;
                CString winName = a; winName.Trim();
                if (val == -2) { Show(w, L"* Usage: /timestamp [-f format | -g format | [-s|-a|-e] on|off|default [window]]", cPart); return; }
                if (target == 0 && winName.IsEmpty()) {   // no target at all: the global switch (as mIRC's own help describes)
                    if (val == -1) { Show(w, L"* Usage: /timestamp on|off (default only applies to a specific window)", cPart); return; }
                    m_tsGlobalOn = (val != 0); SaveTimestamp();
                    Show(w, CString(L"* Timestamps are now globally ") + (m_tsGlobalOn ? L"on" : L"off") + L".", cInfo);
                } else {
                    std::vector<CChatWnd*> targets;
                    if (target == 1) { if (CChatWnd* sw = Status(net)) targets.push_back(sw); }
                    else if (target == 2) { if (auto* aw = dynamic_cast<CChatWnd*>(MDIGetActive())) targets.push_back(aw); }
                    else if (target == 3) { for (auto& kv : m_w) targets.push_back(kv.second); }
                    else if (!winName.IsEmpty()) { if (CChatWnd* nw = winName[0] == L'@' ? Find(nullptr, winName) : Find(net, winName)) targets.push_back(nw); }
                    if (targets.empty()) { Show(w, L"* No matching window.", cPart); return; }
                    for (auto* tw : targets) tw->m_tsMode = val;
                    CString state = val == -1 ? CString(L"following the global setting") : (val ? CString(L"on") : CString(L"off"));
                    CString names; for (auto* tw : targets) names += (names.IsEmpty() ? L"" : L", ") + (tw->m_name == L"*status*" ? CString(L"status") : tw->m_name);
                    Show(w, L"* Timestamps for " + names + L": " + state + L".", cInfo);
                }
            }
        }
        else if (cmd == L"run") {   // /run [-n] file [parameters]: launches a local program or opens a document/URL with its associated app
            CString a = arg; a.TrimLeft(); bool min = false;
            if (a.Left(2).CompareNoCase(L"-n") == 0 && (a.GetLength() == 2 || a[2] == L' ')) { min = true; a = a.Mid(2); a.TrimLeft(); }
            CString file = RunWord(a), params = a;   // whatever's left, unsplit, is passed straight through as the parameters
            if (file.IsEmpty()) { Show(w, L"* Usage: /run [-n] <file> [parameters]", cPart); return; }
            HINSTANCE r = ShellExecuteW(m_hWnd, nullptr, file, params.IsEmpty() ? nullptr : (LPCWSTR)params, nullptr, min ? SW_SHOWMINIMIZED : SW_SHOWNORMAL);
            if ((INT_PTR)r <= 32) {
                wchar_t msg[256]; FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, (DWORD)(INT_PTR)r, 0, msg, 256, nullptr);
                CString e = msg; e.TrimRight(L"\r\n");
                Show(w, L"* /run: couldn't start '" + file + L"' (" + e + L")", cPart);
            }
        }
        else if (cmd == L"away") Send(net, arg.IsEmpty() ? CString(L"AWAY") : L"AWAY :" + arg);
        else if (cmd == L"kick") {   // /kick [#chan] nick [reason]: the reason is sent as trailing text so it can be several words
            CString a = arg, first = Word(a), chan, nick;
            if (!first.IsEmpty() && wcschr(L"#&+!", first[0])) { chan = first; nick = Word(a); }
            else { chan = w->m_chan ? w->m_name : CString(); nick = first; }
            a.Trim();
            if (chan.IsEmpty() || nick.IsEmpty()) Note(net, L"Usage: /kick [#channel] <nick> [reason]", cPart);
            else Send(net, L"KICK " + chan + L" " + nick + (a.IsEmpty() ? CString() : L" :" + a));
        }
        else if (cmd == L"clipboard") { AddtoClipboard(arg); }
        else if (cmd == L"raw" || cmd == L"quote") Send(net, arg);
        else if (cmd == L"help") Note(net, L"/server [-m] host [+port = TLS] (-m connects a second, independent network) /nick /join /part /list [#chan|pattern] [-min N] [-max N] [-n] /msg /query /me /notice /topic /channel /run /colors /logging /timestamp /play /playctrl /dns /window /aline /cline /dline /iline /rline /sline /renwin /timer /timers /identd /tray /tips /tip /titlebar /splay /vol /ctcp /quit /clear /echo /say /alias /unalias /set /unset /unsetall /inc /dec /var /raw; use //cmd to evaluate $identifiers ($me $chan $network $os $date $time $1- ...); other /cmds (mode, kick, whois...) go to the server as-is");
        else { cmd.MakeUpper(); Send(net, cmd + L" " + arg); }
    }

    // ---- server input ----
    void OnLine(Net* net, const CString& raw) {
        CString l = raw, prefix, trail; bool hasT = false;
        if (l.Left(1) == L":") { int sp = l.Find(L' '); if (sp < 0) return; prefix = l.Mid(1, sp - 1); l = l.Mid(sp + 1); }
        int t = l.Find(L" :"); if (t >= 0) { trail = l.Mid(t + 2); l = l.Left(t); hasT = true; }
        std::vector<CString> p; int pos = 0;
        for (CString tok = l.Tokenize(L" ", pos); !tok.IsEmpty(); tok = l.Tokenize(L" ", pos)) p.push_back(tok);
        if (p.empty()) return;
        CString cmd = p[0]; cmd.MakeUpper(); p.erase(p.begin());
        if (hasT) p.push_back(trail);
        auto P = [&](size_t i) { return i < p.size() ? p[i] : CString(); };
        CString nick = prefix, host; int b = nick.Find(L'!');
        if (b >= 0) { host = nick.Mid(b + 1); nick = nick.Left(b); }
        bool me = nick.CompareNoCase(net->nick) == 0;

        if (cmd == L"005") {   // RPL_ISUPPORT: pick out NETWORK=<name> for $network (the line itself still prints below, as before)
            size_t last = hasT ? p.size() - 1 : p.size();   // the trailing "are supported by this server" isn't a token
            for (size_t i = 1; i < last; i++) {
                if (p[i].Left(8).CompareNoCase(L"NETWORK=") == 0) net->network = p[i].Mid(8);
                if (p[i].Left(10).CompareNoCase(L"CHANMODES=") == 0) net->chanmodes = p[i].Mid(10);   // e.g. beI,k,l,imnpst  (a comma-separated four groups)
            }
        }
        if (m_cc && HandleCCNumeric(net, cmd, p)) return;   // 324 mode reply, 367/348/346/728 list entries and their end markers
        if (cmd == L"PING") { Send(net, L"PONG :" + P(0)); }
        else if (cmd == L"PRIVMSG" || cmd == L"NOTICE") {
            CString tgt = P(0), txt = P(1); bool notice = cmd == L"NOTICE";
            bool priv = tgt.CompareNoCase(net->nick) == 0;
            bool ctcp = !txt.IsEmpty() && txt[0] == 1;
            if (ctcp) txt.Trim(CString(wchar_t(1)));
            if (ctcp && txt.Left(6) != L"ACTION") {
                // Any CTCP other than ACTION (VERSION, PING, TIME, and replies to them) is protocol noise,
                // not a conversation: goes to the Status window only, in red, and never opens a query window
                // for the sender  which is what was happening before (every version-scanning bot on a
                // network would silently spawn an empty background window for itself).
                if (notice && txt.Left(5) == L"PING ") {   // reply to a PING we sent via /ctcp: show round-trip time
                    unsigned long sent = wcstoul(txt.Mid(5), nullptr, 10);
                    unsigned long rtt = ::GetTickCount() - sent;
                    CString ms; ms.Format(L"%lu", rtt);
                    Show(Status(net), L"[CTCP PING reply from " + nick + L": " + ms + L"ms]", cPart);
                    return;
                }
                if (!notice) {
                    if (txt == L"VERSION") Send(net, L"NOTICE " + nick + L" :" + CString(wchar_t(1)) + L"VERSION " + CString(VERSION) + CString(wchar_t(1)));
                    else if (txt.Left(4) == L"PING") Send(net, L"NOTICE " + nick + L" :" + CString(wchar_t(1)) + txt + CString(wchar_t(1)));   // echo the payload back, standard CTCP PING reply
                    else if (txt == L"TIME") Send(net, L"NOTICE " + nick + L" :" + CString(wchar_t(1)) + L"TIME " + CTime::GetCurrentTime().Format(L"%a %b %d %H:%M:%S %Y") + CString(wchar_t(1)));
                    else if (txt == L"FINGER") Send(net, L"NOTICE " + nick + L" :" + CString(wchar_t(1)) + L"FINGER " + CString(VERSION) + CString(wchar_t(1)));
                }
                //this is to let you know that someone CTCPed you.    
                //might we should reply to unknown CTCPs
                Show(Status(net), L"[CTCP " + txt + L" from " + nick + L"]", cCTCP);
                return;
            }
            CChatWnd* w = (notice && (priv || !Find(net, tgt))) ? Status(net) : (priv ? OpenBg(net, nick) : Open(net, tgt, IsChan(tgt)));
            if (ctcp) Show(w, L"* " + nick + txt.Mid(6), cAction);   // ACTION (/me): a real chat message, so it still uses the normal window
            else if (notice) Show(w, L"-" + (nick.IsEmpty() ? prefix : nick) + L"- " + txt, cNotice);
            else Show(w, L"<" + nick + L"> " + txt);
            if (!notice) {   // see Tips: only real messages (including /me) trigger a balloon, never notices/CTCP noise
                CString tipText = ctcp ? (nick + L" " + txt.Mid(6)) : (L"<" + nick + L"> " + txt);
                if (priv && m_tipsPrivate) QueueEventTip(nick, tipText, w);
                else if (!priv && IsChan(tgt) && m_tipsChannel) QueueEventTip(tgt, tipText, w);
            }
        }
        //* Someone (user@hostname) invites you to join #chan
        else if (cmd == L"INVITE") { Note(net, nick + L" invites you to join " + P(1), cInvite); }
        else if (cmd == L"JOIN") {
            CString ch = P(0); CChatWnd* w = me ? Open(net, ch, true) : Find(net, ch); if (!w) return;
            if (!me) w->AddNick(nick);
            Show(w, L"* " + nick + L" (" + host + L") has joined " + ch, cJoin);
        }
        else if (cmd == L"PART") {
            if (me) { Drop(net, P(0)); return; }
            if (CChatWnd* w = Find(net, P(0))) { w->DelNick(nick); Show(w, L"* " + nick + L" has left " + P(0) + L" (" + P(1) + L")", cPart); }
        }
        else if (cmd == L"KICK") {
            if (P(1).CompareNoCase(net->nick) == 0) { Note(net, L"You were kicked from " + P(0) + L" by " + nick + L" (" + P(2) + L")", cKick); Drop(net, P(0)); return; }
            if (CChatWnd* w = Find(net, P(0))) { w->DelNick(P(1)); Show(w, L"* " + P(1) + L" was kicked by " + nick + L" (" + P(2) + L")", cKick); }
        }
        else if (cmd == L"QUIT") {
            for (auto& kv : m_w) {
                CChatWnd* w = kv.second; if (w->net != net) continue;
                if (w->DelNick(nick) || (!w->m_chan && w->m_name.CompareNoCase(nick) == 0))
                    Show(w, L"* " + nick + L" has quit (" + P(0) + L")", cQuit);
            }
        }
        else if (cmd == L"NICK") {
            CString nn = P(0); if (me) net->nick = nn;
            for (auto& kv : m_w) {
                CChatWnd* w = kv.second; if (w->net != net) continue;
                if (w->DelNick(nick)) { w->AddNick(nn); Show(w, L"* " + nick + L" is now known as " + nn, cNickname); }
            }
        }
        else if (cmd == L"TOPIC") {
            if (CChatWnd* w = Find(net, P(0))) { w->SetTopic(P(1)); Show(w, L"* " + nick + L" changed the topic to: " + P(1), cTopic); }
        }
        else if (cmd == L"MODE") {
            CChatWnd* w = Find(net, P(0)); CString m; for (size_t i = 1; i < p.size(); i++) m += p[i] + L" ";
            Show(w ? w : Status(net), L"* " + nick + L" sets mode " + m, cMode);
            if (w && p.size() > 2) { w->m_refresh = true; Send(net, L"NAMES " + P(0)); }
        }
        else if (cmd == L"001") { net->nick = P(0); Note(net, P(1), cText); SetState(net, L"Connected: " + (prefix.IsEmpty() ? net->o.host : prefix) + (net->o.tls ? L" (TLS)" : L""));
            if (!net->o.autojoin.IsEmpty()) Send(net, L"JOIN " + net->o.autojoin); }
        else if (cmd == L"332") { if (CChatWnd* w = Find(net, P(1))) { w->SetTopic(P(2)); Show(w, L"* Topic is " + P(2), cTopic); } }
        else if (cmd == L"333") {   // RPL_TOPICWHOTIME: channel setter unixtimestamp -- who set the topic and when, shown right after the topic itself
            CString chan = P(1), who = P(2), ts = P(3), when;
            if (IsAllDigits(ts)) { CTime ct((time_t)_wtoi64(ts)); when = ct.Format(L"%a %b %d %H:%M:%S %Y"); }
            CChatWnd* w = Find(net, chan);
            Show(w ? w : Status(net), L"* Set by " + who + (when.IsEmpty() ? CString() : L" on " + when), cTopic);
        }
        else if (cmd == L"321") { /* RPL_LISTSTART header ("Channel Users Name"): nothing to do, our list window has its own column headers */ }
        else if (cmd == L"322") {   // RPL_LIST: <chan> <#users> :<topic> (topic may have a leading "[+modes]" prefix)
            if (net->listWnd) { CString modes, topic; ParseListModes(P(3), modes, topic); net->listWnd->AddRow(P(1), _wtoi(P(2)), modes, topic); }
        }
        else if (cmd == L"323") { if (net->listWnd) net->listWnd->Resort(); }   // RPL_LISTEND: results complete, sort and display
        else if (cmd == L"353") {
            if (CChatWnd* w = Find(net, P(2))) {
                if (w->m_refresh) { w->ClearNicks(); w->m_refresh = false; }
                int q = 0; CString names = P(3);
                for (CString n = names.Tokenize(L" ", q); !n.IsEmpty(); n = names.Tokenize(L" ", q)) w->AddNick(n);
            }
        }
        else if (cmd == L"366") {}
        else if (cmd == L"433") { net->nick += L"_"; Note(net, L"Nickname in use, trying " + net->nick, cText); Send(net, L"NICK " + net->nick); }
        //whois stuff
        else if (cmd == L"311") { Note(net, P(1) + L" is " + P(2) + L"@" + P(3) + (P(5).IsEmpty() ? CString() : L" * " + P(5)), cWhois); }   // RPL_WHOISUSER: nick user host * :realname
        else if (cmd == L"312") { Note(net, P(1) + L" is on server " + P(2) + (P(3).IsEmpty() ? CString() : L" " + P(3)), cWhois); } // RPL_WHOISSERVER
        else if (cmd == L"317") {   // RPL_WHOISIDLE: nick idle [signon] :seconds idle, signon time
            long idle = _wtol(P(2));
            CString s; s.Format(L"%s has been idle for %ldh %ldm %lds", (LPCWSTR)P(1), idle / 3600, (idle / 60) % 60, idle % 60);
            CString signon = P(3);
            if (!signon.IsEmpty() && IsAllDigits(signon)) { CTime ct((time_t)_wtoi64(signon)); s += L", signed on " + ct.Format(L"%a %b %d %H:%M:%S %Y"); }
            Note(net, s, cWhois);
        }
        else if (cmd == L"318") { Note(net, P(1) + L" End of /WHOIS list.", cWhois); }  //End of /WHOIS list. //-- End of WHOIS -- // RPL_ENDOFWHOIS
        else if (cmd == L"319") { Note(net, P(1) + L" is on channels: " + P(2), cWhois); }  // RPL_WHOISCHANNELS
        else if (cmd == L"301" || cmd == L"313" || cmd == L"330" || cmd == L"338" || cmd == L"378" || cmd == L"379" || cmd == L"671") {
            // other common WHOIS-block lines (away, IRC operator, logged-in-as, actual host, connecting-from, user modes,
            // secure connection -- numbers and exact wording vary by server); joined the same way the old generic
            // fallback did, just consistently colored with the rest of the WHOIS block instead of falling through to it
            CString j; for (size_t i = 1; i < p.size(); i++) j += p[i] + L" ";
            Note(net, j.IsEmpty() ? raw : j, cWhois);
        }
        else if (cmd == L"302" && !m_pendingUserhost.empty()) {   // RPL_USERHOST: nick[*]=+ident@host, space-separated; only relevant here for a pending /dns nickname lookup
            CString trailing = P(1); int tp = 0;
            for (CString entry = trailing.Tokenize(L" ", tp); !entry.IsEmpty(); entry = trailing.Tokenize(L" ", tp)) {
                int eq = entry.Find(L'='); if (eq < 0) continue;
                CString nk = entry.Left(eq); if (!nk.IsEmpty() && nk[nk.GetLength() - 1] == L'*') nk = nk.Left(nk.GetLength() - 1);
                CString rest = entry.Mid(eq + 1); int at = rest.Find(L'@'); if (at < 0) continue;
                CString host = rest.Mid(at + 1);
                CString key = nk; key.MakeLower();
                auto it = m_pendingUserhost.find(key);
                if (it != m_pendingUserhost.end()) {
                    int reqId = it->second; m_pendingUserhost.erase(it);
                    for (auto& r : m_dnsQueue) if (r.id == reqId) { r.isNickname = false; r.resolvedHost = host; r.status = L"queued"; break; }
                }
            }
            StartNextDnsIfIdle();
        }
        else { CString j; for (size_t i = 1; i < p.size(); i++) j += p[i] + L" "; Note(net, j.IsEmpty() ? raw : cmd + ": " + j, cText); }
    }
    BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext*) override { return CreateClient(lpcs, nullptr); }
    void MakeFont(LOGFONT& lf, const CString& face, int pt, bool bold, bool italic) {
        ZeroMemory(&lf, sizeof lf);
        CClientDC dc(this);
        lf.lfHeight = -MulDiv(pt, dc.GetDeviceCaps(LOGPIXELSY), 72);
        lf.lfWeight = bold ? FW_BOLD : FW_NORMAL; lf.lfItalic = italic;
        lf.lfCharSet = DEFAULT_CHARSET; lf.lfOutPrecision = OUT_DEFAULT_PRECIS; lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY; lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        wcsncpy_s(lf.lfFaceName, face.IsEmpty() ? CString(DEFAULT_FONT) : face, LF_FACESIZE - 1);
    }
    void LoadFont() {
        CWinApp* a = AfxGetApp();
        CString face = a->GetProfileString(L"Font", L"Face", CString(DEFAULT_FONT));
        int pt = a->GetProfileInt(L"Font", L"Size", 10);
        MakeFont(m_chatFont, face, pt, a->GetProfileInt(L"Font", L"Bold", 0) != 0, a->GetProfileInt(L"Font", L"Italic", 0) != 0);
    }
    void SaveFont() {
        CWinApp* a = AfxGetApp(); CClientDC dc(this);
        int pt = -MulDiv(m_chatFont.lfHeight, 72, dc.GetDeviceCaps(LOGPIXELSY));
        a->WriteProfileString(L"Font", L"Face", m_chatFont.lfFaceName); a->WriteProfileInt(L"Font", L"Size", pt);
        a->WriteProfileInt(L"Font", L"Bold", m_chatFont.lfWeight >= FW_BOLD); a->WriteProfileInt(L"Font", L"Italic", m_chatFont.lfItalic);
    }
    static CString ResolveSkinPath(const CString& p) {   // a relative path (e.g. "images\skin.png") is resolved against the exe's own folder
        if (p.IsEmpty() || (p.GetLength() >= 2 && p[1] == L':') || p.Left(2) == L"\\\\") return p;   // already absolute or UNC
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        CString dir = exe; dir = dir.Left(dir.ReverseFind(L'\\') + 1);
        return dir + p;
    }
    static CString RelativizeSkinPath(const CString& abs) {   // stores paths under the exe's folder as relative, matching irc.ini's format
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        CString dir = exe; dir = dir.Left(dir.ReverseFind(L'\\') + 1);
        CString a = abs, d = dir; a.MakeLower(); d.MakeLower();
        return (a.Left(d.GetLength()) == d) ? abs.Mid(dir.GetLength()) : abs;
    }
    // ---- Logging: chat history saved to disk, one file per network+window (see CLoggingDlg) ----
    static CString SanitizeFileName(CString s) {   // strips characters Windows won't allow in a file name
        CString o; for (int i = 0; i < s.GetLength(); i++) { wchar_t c = s[i]; o += wcschr(L"\\/:*?\"<>|", c) ? L'_' : c; }
        o.Trim(); return o.IsEmpty() ? CString(L"_") : o;
    }
    void LoadLogging() {
        CWinApp* a = AfxGetApp();
        m_logEnabled = a->GetProfileInt(L"Logging", L"enabled", 0) != 0;
        m_logFolder = a->GetProfileString(L"Logging", L"folder", IniPath(L"logs"));
        if (m_logEnabled && !m_logFolder.IsEmpty()) SHCreateDirectoryExW(nullptr, m_logFolder, nullptr);
    }
    void SaveLogging() {
        CWinApp* a = AfxGetApp();
        a->WriteProfileInt(L"Logging", L"enabled", m_logEnabled ? 1 : 0);
        a->WriteProfileString(L"Logging", L"folder", m_logFolder);
    }
    void LoadTimestamp() {
        CWinApp* a = AfxGetApp();
        m_tsGlobalOn = a->GetProfileInt(L"Timestamp", L"global", 1) != 0;
        m_tsEventFmt = a->GetProfileString(L"Timestamp", L"eventfmt", L"[HH:nn]");
        m_tsLogFmt = a->GetProfileString(L"Timestamp", L"logfmt", L"[HH:nn:ss]");
    }
    void SaveTimestamp() {
        CWinApp* a = AfxGetApp();
        a->WriteProfileInt(L"Timestamp", L"global", m_tsGlobalOn ? 1 : 0);
        a->WriteProfileString(L"Timestamp", L"eventfmt", m_tsEventFmt);
        a->WriteProfileString(L"Timestamp", L"logfmt", m_tsLogFmt);
    }
    // ---- Online Timer: persistence and live state ----
    static CString OtFormatDate(const CTime& t) { return t.GetTime() <= 0 ? CString() : t.Format(L"%a %b %d %H:%M:%S %Y"); }
    static CTime OtParseDate(const CString& s) {
        int y, d, h, mi, se; wchar_t wk[8] = {}, mn[8] = {};
        if (swscanf_s(s, L"%3s %3s %d %d:%d:%d %d", wk, (unsigned)_countof(wk), mn, (unsigned)_countof(mn), &d, &h, &mi, &se, &y) != 7) return CTime((time_t)0);
        static const wchar_t* mons[12] = { L"Jan",L"Feb",L"Mar",L"Apr",L"May",L"Jun",L"Jul",L"Aug",L"Sep",L"Oct",L"Nov",L"Dec" };
        int mnum = 1; for (int i = 0; i < 12; i++) if (_wcsicmp(mn, mons[i]) == 0) { mnum = i + 1; break; }
        return CTime(y, mnum, d, h, mi, se);
    }
    void LoadOnlineTimer() {
        CWinApp* a = AfxGetApp();
        m_otEnabled = a->GetProfileInt(L"OnlineTimer", L"enabled", 1) != 0;
        m_otShowTotal = a->GetProfileInt(L"OnlineTimer", L"showTotal", 1) != 0;
        m_otTotalBanked = _wtof(a->GetProfileString(L"OnlineTimer", L"totalSeconds", L"0"));
        m_otTotalResetTime = OtParseDate(a->GetProfileString(L"OnlineTimer", L"totalResetDate", L""));
    }
    void SaveOnlineTimer() {
        CWinApp* a = AfxGetApp();
        a->WriteProfileInt(L"OnlineTimer", L"enabled", m_otEnabled ? 1 : 0);
        a->WriteProfileInt(L"OnlineTimer", L"showTotal", m_otShowTotal ? 1 : 0);
        CString secs; secs.Format(L"%.0f", m_otTotalBanked);
        a->WriteProfileString(L"OnlineTimer", L"totalSeconds", secs);
        a->WriteProfileString(L"OnlineTimer", L"totalResetDate", OtFormatDate(m_otTotalResetTime));
    }
    double OtCurrentSeconds() const { return m_otSessionStart ? (GetTickCount64() - m_otSessionStart) / 1000.0 : 0; }
    double OtTotalSeconds() const { return m_otTotalBanked + OtCurrentSeconds(); }
    void OtResetCurrent() { if (m_otSessionStart) m_otSessionStart = GetTickCount64(); m_otSessionResetTime = CTime::GetCurrentTime(); }
    void OtResetTotal() { m_otTotalBanked = 0; m_otTotalResetTime = CTime::GetCurrentTime(); SaveOnlineTimer(); }
    // Called from the regular ~500ms UI tick: banks a finished session into the total the moment every network
    // disconnects, starts a fresh session the moment any network (re)connects, and keeps the status window
    // titlebar(s) showing the live time when enabled.
    void UpdateOnlineTimer() {
        bool anyConn = false; for (auto& np : m_nets) if (np->conn) { anyConn = true; break; }
        if (anyConn && !m_otSessionStart) { m_otSessionStart = GetTickCount64(); m_otSessionResetTime = CTime::GetCurrentTime(); }
        else if (!anyConn && m_otSessionStart) { m_otTotalBanked += OtCurrentSeconds(); m_otSessionStart = 0; SaveOnlineTimer(); }
        for (auto& kv : m_w) {
            CChatWnd* sw = kv.second; if (sw->m_name != L"*status*") continue;
            if (!m_otEnabled) { sw->SetWindowText(L"Status"); continue; }
            double secs = m_otShowTotal ? OtTotalSeconds() : OtCurrentSeconds();
            sw->SetWindowText(L"Status - [" + COnlineTimerDlg::FormatElapsed(secs) + L"]");
        }
    }
    afx_msg void OnClose() {   // banks whatever's left of the current online-timer session before the app actually closes
        if (m_otSessionStart) { m_otTotalBanked += OtCurrentSeconds(); m_otSessionStart = 0; }
        SaveOnlineTimer();
        StopIdentd();
        CloseSoundChannel(m_waveChan); CloseSoundChannel(m_midiChan); CloseSoundChannel(m_mp3Chan);
        // "If you hold down the Shift key when you quit mIRC, the next time you run it, it will be minimized."
        AfxGetApp()->WriteProfileInt(L"Tray", L"startMinimizedNext", (::GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0);
        HideTrayIcon();
        CMDIFrameWnd::OnClose();
    }
    // SC_MINIMIZE is intercepted so minimizing can go straight to the tray instead of the taskbar, per the
    // "Place mIRC in tray when minimized" setting -- and Shift, held during the click, always forces tray-minimize
    // even when that setting is off, matching mIRC's own described override (the reverse direction -- forcing a
    // normal taskbar minimize while the setting is on -- isn't described in the Tray help text, so isn't assumed here).
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam) {
        if ((nID & 0xFFF0) == SC_MINIMIZE) {
            bool shiftHeld = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (m_trayOnMinimize || shiftHeld) {
                ShowWindow(SW_MINIMIZE); ShowWindow(SW_HIDE);
                if (!m_trayAlwaysShow) ShowTrayIcon();
                return;
            }
        }
        CMDIFrameWnd::OnSysCommand(nID, lParam);
    }
    // ---- System tray ----
    void LoadTraySettings() {
        CWinApp* a = AfxGetApp();
        m_trayAlwaysShow = a->GetProfileInt(L"Tray", L"alwaysShow", 0) != 0;
        m_trayMinOnStartup = a->GetProfileInt(L"Tray", L"minOnStartup", 0) != 0;
        m_trayOnMinimize = a->GetProfileInt(L"Tray", L"onMinimize", 0) != 0;
        m_trayAnimate = a->GetProfileInt(L"Tray", L"animate", 1) != 0;
        m_traySingleClick = a->GetProfileInt(L"Tray", L"singleClick", 0) != 0;
        m_trayIconPath = a->GetProfileString(L"Tray", L"iconPath", L"");
        m_trayIconIndex = a->GetProfileInt(L"Tray", L"iconIndex", 0);
    }
    void SaveTraySettings() {
        CWinApp* a = AfxGetApp();
        a->WriteProfileInt(L"Tray", L"alwaysShow", m_trayAlwaysShow ? 1 : 0);
        a->WriteProfileInt(L"Tray", L"minOnStartup", m_trayMinOnStartup ? 1 : 0);
        a->WriteProfileInt(L"Tray", L"onMinimize", m_trayOnMinimize ? 1 : 0);
        a->WriteProfileInt(L"Tray", L"animate", m_trayAnimate ? 1 : 0);
        a->WriteProfileInt(L"Tray", L"singleClick", m_traySingleClick ? 1 : 0);
        a->WriteProfileString(L"Tray", L"iconPath", m_trayIconPath);
        a->WriteProfileInt(L"Tray", L"iconIndex", m_trayIconIndex);
    }
    static HICON LoadIconFromFile(const CString& path, int index) {
        HICON h = ::ExtractIconW(AfxGetInstanceHandle(), path, index);
        return (h && h != (HICON)1) ? h : nullptr;   // ExtractIcon returns (HICON)1 for "file has icons but not at this index", NULL for "couldn't open it at all"
    }
    static HICON AppDefaultIcon() {
        HICON h = (HICON)::LoadImageW(AfxGetInstanceHandle(), MAKEINTRESOURCE(101), IMAGE_ICON, 16, 16, LR_DEFAULTSIZE);
        return h ? h : ::LoadIcon(nullptr, IDI_APPLICATION);   // falls back to a generic system icon when IRC.rc wasn't linked in
    }
    HICON MakeTrayAlertIcon() {   // a simple, deliberately plain "something happened" alternate frame -- not mIRC's own multi-phase animation (purple when connected, a revolving planet while connecting), which would need several custom-drawn icon frames this client doesn't have
        if (m_trayAlertIcon) return m_trayAlertIcon;
        HDC scr = ::GetDC(nullptr);
        HDC mem = ::CreateCompatibleDC(scr);
        HBITMAP color = ::CreateCompatibleBitmap(scr, 16, 16);
        HBITMAP oldBmp = (HBITMAP)::SelectObject(mem, color);
        RECT r = { 0, 0, 16, 16 };
        HBRUSH bg = ::CreateSolidBrush(RGB(230, 30, 30)); ::FillRect(mem, &r, bg); ::DeleteObject(bg);
        HBRUSH dot = ::CreateSolidBrush(RGB(255, 230, 0)); HGDIOBJ oldBr = ::SelectObject(mem, dot);
        HGDIOBJ oldPen = ::SelectObject(mem, ::GetStockObject(NULL_PEN));
        ::Ellipse(mem, 3, 3, 13, 13);
        ::SelectObject(mem, oldPen); ::SelectObject(mem, oldBr); ::DeleteObject(dot);
        ::SelectObject(mem, oldBmp);
        HBITMAP mask = ::CreateBitmap(16, 16, 1, 1, nullptr);
        HDC maskDc = ::CreateCompatibleDC(scr); HBITMAP oldMask = (HBITMAP)::SelectObject(maskDc, mask);
        ::PatBlt(maskDc, 0, 0, 16, 16, BLACKNESS);   // an all-zero AND-mask: the icon is fully opaque, no transparency needed for a plain square
        ::SelectObject(maskDc, oldMask);
        ICONINFO ii = {}; ii.fIcon = TRUE; ii.hbmColor = color; ii.hbmMask = mask;
        m_trayAlertIcon = ::CreateIconIndirect(&ii);
        ::DeleteObject(color); ::DeleteObject(mask); ::DeleteDC(mem); ::DeleteDC(maskDc); ::ReleaseDC(nullptr, scr);
        return m_trayAlertIcon;
    }
    bool AnyWindowHasActivity() const { for (auto& kv : m_w) if (kv.second->m_act > 0) return true; return false; }
    void SetTrayIconHandle(HICON h) {
        m_trayNid.hIcon = h;
        if (m_trayIconAdded) ::Shell_NotifyIconW(NIM_MODIFY, &m_trayNid);
    }
    void ShowTrayIcon() {
        if (m_trayIconAdded) return;
        m_trayIsCustomIcon = !m_trayIconPath.IsEmpty();
        m_trayIconHandle = m_trayIsCustomIcon ? LoadIconFromFile(m_trayIconPath, m_trayIconIndex) : nullptr;
        if (!m_trayIconHandle) { m_trayIconHandle = AppDefaultIcon(); m_trayIsCustomIcon = false; }
        ZeroMemory(&m_trayNid, sizeof(m_trayNid));
        m_trayNid.cbSize = sizeof(m_trayNid); m_trayNid.hWnd = m_hWnd; m_trayNid.uID = 1;
        m_trayNid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_trayNid.uCallbackMessage = WM_APP + 52;
        m_trayNid.hIcon = m_trayIconHandle;
        wcscpy_s(m_trayNid.szTip, L"IRC Client");
        ::Shell_NotifyIconW(NIM_ADD, &m_trayNid);
        m_trayIconAdded = true;
        if (m_trayAnimate && !m_trayIsCustomIcon && !m_trayAnimTimerId) m_trayAnimTimerId = SetTimer(2003, 600, nullptr);   // mIRC's own note: animation doesn't apply when a custom icon is selected
    }
    void HideTrayIcon() {
        if (m_trayAnimTimerId) { KillTimer(m_trayAnimTimerId); m_trayAnimTimerId = 0; }
        if (!m_trayIconAdded) return;
        ::Shell_NotifyIconW(NIM_DELETE, &m_trayNid);
        m_trayIconAdded = false; m_trayFlashOn = false;
        if (m_trayIsCustomIcon && m_trayIconHandle) { ::DestroyIcon(m_trayIconHandle); m_trayIconHandle = nullptr; }
    }
    void TrayAnimTick() {   // alternates the icon between normal and the alert frame, only while something has unread activity
        if (!m_trayIconAdded) return;
        if (!AnyWindowHasActivity()) { if (m_trayFlashOn) { SetTrayIconHandle(m_trayIconHandle); m_trayFlashOn = false; } return; }
        m_trayFlashOn = !m_trayFlashOn;
        SetTrayIconHandle(m_trayFlashOn ? MakeTrayAlertIcon() : m_trayIconHandle);
    }
    void ToggleMainWindowFromTray() {
        if (IsIconic() || !IsWindowVisible()) {
            ShowWindow(SW_RESTORE); SetForegroundWindow();
            if (!m_trayAlwaysShow) HideTrayIcon();
        } else {
            ShowWindow(SW_MINIMIZE); ShowWindow(SW_HIDE);
            if (!m_trayAlwaysShow) ShowTrayIcon();
        }
    }
    void ShowTrayMenu() {
        enum { ID_OPEN = 25001, ID_EXIT = 25002, ID_WINBASE = 25100 };
        CMenu m; m.CreatePopupMenu();
        int id = ID_WINBASE; std::map<int, CChatWnd*> map;
        for (auto& kv : m_w) {
            CChatWnd* cw = kv.second;
            CString label = cw->m_name == L"*status*" ? CString(L"Status") : cw->m_name;
            m.AppendMenu(MF_STRING | (cw->m_act > 0 ? MF_CHECKED : 0), id, label);
            map[id] = cw; id++;
        }
        if (!m_w.empty()) m.AppendMenu(MF_SEPARATOR);
        m.AppendMenu(MF_STRING, ID_OPEN, L"Open IRC Client");
        m.AppendMenu(MF_STRING, ID_EXIT, L"Exit");
        SetForegroundWindow(); m_menuOpen = true;   // SetForegroundWindow here is required for the popup to dismiss correctly on an outside click, per the standard tray-menu pattern
        CPoint pt; GetCursorPos(&pt);
        int cmd = m.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, this);
        m_menuOpen = false; PostMessage(WM_NULL, 0, 0);
        if (cmd == ID_OPEN) { ShowWindow(SW_RESTORE); SetForegroundWindow(); if (!m_trayAlwaysShow) HideTrayIcon(); }
        else if (cmd == ID_EXIT) PostMessage(WM_CLOSE);
        else if (map.count(cmd)) { ShowWindow(SW_RESTORE); SetForegroundWindow(); if (!m_trayAlwaysShow) HideTrayIcon(); Activate(map[cmd]); }
    }
    afx_msg LRESULT OnTrayNotify(WPARAM, LPARAM lp) {
        UINT msg = (UINT)lp;
        if (msg == WM_LBUTTONDOWN && m_traySingleClick) ToggleMainWindowFromTray();
        else if (msg == WM_LBUTTONDBLCLK && !m_traySingleClick) ToggleMainWindowFromTray();
        else if (msg == WM_RBUTTONUP) ShowTrayMenu();
        else if (msg == NIN_BALLOONUSERCLICK) OnTipClicked();
        else if (msg == NIN_BALLOONTIMEOUT) { if (!m_tipQueue.empty()) m_tipQueue.erase(m_tipQueue.begin()); }
        return 0;
    }
    void OnTrayDialog() {
        CTrayDlg dlg(m_trayAlwaysShow, m_trayMinOnStartup, m_trayOnMinimize, m_trayAnimate, m_traySingleClick, m_trayIconPath, m_trayIconIndex, this);
        dlg.getDefaultIcon = [] { return AppDefaultIcon(); };
        dlg.loadIconFrom = [](const CString& p, int i) { return LoadIconFromFile(p, i); };
        if (dlg.DoModal() != IDOK) return;
        m_trayAlwaysShow = dlg.alwaysShow; m_trayMinOnStartup = dlg.startMin; m_trayOnMinimize = dlg.onMin;
        m_trayAnimate = dlg.animate; m_traySingleClick = dlg.singleClick; m_trayIconPath = dlg.iconPath; m_trayIconIndex = dlg.iconIndex;
        SaveTraySettings();
        if (m_trayIconAdded) { HideTrayIcon(); ShowTrayIcon(); }   // picks up a changed icon/animate setting immediately
        else if (m_trayAlwaysShow) ShowTrayIcon();
    }
    void CmdTray(CChatWnd* w, CString arg) {   // /tray -iNmNsNtNaN <filename>: -i sets the icon index, -m/-s/-t/-a toggle the same four dialog settings a /tray-only switch can reach
        arg.Trim();
        if (arg.Left(1) == L"-") {
            int i = 1; for (; i < arg.GetLength() && arg[i] != L' '; i++) {
                wchar_t c = arg[i];
                if (c == L'i' || c == L'm' || c == L's' || c == L't' || c == L'a') {
                    CString digs; while (i + 1 < arg.GetLength() && iswdigit(arg[i + 1])) digs += arg[++i];
                    int v = digs.IsEmpty() ? 0 : _wtoi(digs);
                    if (c == L'i') m_trayIconIndex = v;
                    else if (c == L'm') m_trayAlwaysShow = v != 0;
                    else if (c == L's') m_traySingleClick = v != 0;
                    else if (c == L't') m_trayOnMinimize = v != 0;
                    else if (c == L'a') m_trayAnimate = v != 0;
                }
            }
            arg = arg.Mid(i); arg.TrimLeft();
        }
        if (!arg.IsEmpty()) m_trayIconPath = arg;
        SaveTraySettings();
        if (m_trayIconAdded) { HideTrayIcon(); ShowTrayIcon(); } else if (m_trayAlwaysShow) ShowTrayIcon();
        Show(w, L"* Tray settings updated.", cInfo);
    }
    // ---- Tips ----
    void LoadTipsSettings() {
        CWinApp* a = AfxGetApp();
        m_tipsOn = a->GetProfileInt(L"Tips", L"on", 1) != 0;
        m_tipsChannel = a->GetProfileInt(L"Tips", L"channel", 1) != 0;
        m_tipsPrivate = a->GetProfileInt(L"Tips", L"private", 1) != 0;
        m_tipsOther = a->GetProfileInt(L"Tips", L"other", 1) != 0;
        m_tipsHideFullscreen = a->GetProfileInt(L"Tips", L"hideFullscreen", 1) != 0;
        m_tipsQueueSize = a->GetProfileInt(L"Tips", L"queueSize", 5);
        m_tipsDisplayTime = a->GetProfileInt(L"Tips", L"displayTime", 10);
    }
    void SaveTipsSettings() {
        CWinApp* a = AfxGetApp();
        a->WriteProfileInt(L"Tips", L"on", m_tipsOn ? 1 : 0);
        a->WriteProfileInt(L"Tips", L"channel", m_tipsChannel ? 1 : 0);
        a->WriteProfileInt(L"Tips", L"private", m_tipsPrivate ? 1 : 0);
        a->WriteProfileInt(L"Tips", L"other", m_tipsOther ? 1 : 0);
        a->WriteProfileInt(L"Tips", L"hideFullscreen", m_tipsHideFullscreen ? 1 : 0);
        a->WriteProfileInt(L"Tips", L"queueSize", m_tipsQueueSize);
        a->WriteProfileInt(L"Tips", L"displayTime", m_tipsDisplayTime);
    }
    bool IsAppActive() const { HWND fg = ::GetForegroundWindow(); return fg == m_hWnd || ::IsChild(m_hWnd, fg); }
    bool IsOtherAppFullscreen() const {   // a simple, standard heuristic: some other app's foreground window exactly covers its monitor
        HWND fg = ::GetForegroundWindow();
        if (!fg || fg == m_hWnd || ::IsChild(m_hWnd, fg)) return false;
        RECT wr; if (!::GetWindowRect(fg, &wr)) return false;
        HMONITOR mon = ::MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) }; if (!::GetMonitorInfo(mon, &mi)) return false;
        return wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top && wr.right >= mi.rcMonitor.right && wr.bottom >= mi.rcMonitor.bottom;
    }
    CChatWnd* FindWindowBySeq(int wid) { if (!wid) return nullptr; for (auto& kv : m_w) if (kv.second->m_seq == wid) return kv.second; return nullptr; }
    void ClearTipBalloon() {   // an empty info text is the standard way to dismiss/hide whatever balloon is currently shown
        if (!m_trayIconAdded) return;
        NOTIFYICONDATAW nid = m_trayNid; nid.uFlags = NIF_INFO; nid.szInfo[0] = 0; nid.szInfoTitle[0] = 0;
        ::Shell_NotifyIconW(NIM_MODIFY, &nid);
    }
    void ShowTipBalloon(const TipInfo& t) {
        if (!m_trayIconAdded) ShowTrayIcon();
        NOTIFYICONDATAW nid = m_trayNid;
        nid.uFlags = NIF_INFO;
        wcsncpy_s(nid.szInfoTitle, t.title, _TRUNCATE); wcsncpy_s(nid.szInfo, t.text, _TRUNCATE);
        nid.dwInfoFlags = NIIF_INFO;
        nid.uTimeout = 10000;   // modern Windows ignores this and manages its own balloon lifetime -- TipTick (below) is what actually enforces the configured display time
        ::Shell_NotifyIconW(NIM_MODIFY, &nid);
    }
    void TipTick() {   // advances the queue: shows the head tip once mIRC isn't active, and pops it once its time is up
        if (m_tipQueue.empty()) return;
        if (m_tipsHideFullscreen && IsOtherAppFullscreen()) return;
        TipInfo& head = m_tipQueue.front();
        if (head.shownAt == 0) {
            if (!m_tipsOn || IsAppActive()) return;   // "tips only appear when mIRC is not the active application"
            ShowTipBalloon(head); head.shownAt = GetTickCount64();
            return;
        }
        if (head.delaySec >= 0 && (GetTickCount64() - head.shownAt) >= (ULONGLONG)head.delaySec * 1000) {
            m_tipQueue.erase(m_tipQueue.begin());
            TipTick();   // try the next one immediately, in the same tick
        }
    }
    void TipCheckActivation() {   // "hidden the moment mIRC becomes active"
        bool active = IsAppActive();
        if (active && !m_tipsAppWasActive && !m_tipQueue.empty() && m_tipQueue.front().shownAt != 0) {
            ClearTipBalloon(); m_tipQueue.erase(m_tipQueue.begin());
        }
        m_tipsAppWasActive = active;
    }
    void AddTipToQueue(TipInfo t) {
        if ((int)m_tipQueue.size() >= m_tipsQueueSize && !m_tipQueue.empty()) {
            if (m_tipQueue.front().shownAt != 0) ClearTipBalloon();
            m_tipQueue.erase(m_tipQueue.begin());   // "the oldest tip is removed" to make room
        }
        m_tipQueue.push_back(t);
        TipTick();
    }
    void QueueEventTip(const CString& title, const CString& text, CChatWnd* w) {
        if (!m_tipsOn) return;
        TipInfo t; t.seq = ++m_tipSeq; t.name.Format(L"auto%d", t.seq);
        t.title = title; t.text = text; t.delaySec = m_tipsDisplayTime; t.wid = w ? w->m_seq : 0;
        AddTipToQueue(t);
    }
    void OnTipClicked() {
        if (m_tipQueue.empty()) return;
        TipInfo t = m_tipQueue.front(); m_tipQueue.erase(m_tipQueue.begin());
        if ((::GetKeyState(VK_SHIFT) & 0x8000) != 0) return;   // Shift-click: dismiss only, already removed above
        CChatWnd* w = FindWindowBySeq(t.wid);
        if (!t.alias.IsEmpty()) { if (!w) w = m_w.empty() ? nullptr : m_w.begin()->second; if (w) RunScript(w, std::vector<CString>{ t.alias }, CString()); }
        else if (w) { ShowWindow(SW_RESTORE); SetForegroundWindow(); if (!m_trayAlwaysShow) HideTrayIcon(); Activate(w); }
    }
    void OnTipsDialog() {
        CTipsDlg dlg(m_tipsChannel, m_tipsPrivate, m_tipsOther, m_tipsQueueSize, m_tipsDisplayTime, m_tipsHideFullscreen, this);
        if (dlg.DoModal() != IDOK) return;
        m_tipsChannel = dlg.chanOn; m_tipsPrivate = dlg.privOn; m_tipsOther = dlg.otherOn;
        m_tipsQueueSize = dlg.queueSize; m_tipsDisplayTime = dlg.displayTime; m_tipsHideFullscreen = dlg.hideFullscreen;
        SaveTipsSettings();
    }
    void CmdTips(CChatWnd* w, CString arg) {
        arg.Trim(); CString a = arg; a.MakeLower();
        if (a == L"on") { m_tipsOn = true; SaveTipsSettings(); Show(w, L"* Tips on.", cInfo); }
        else if (a == L"off") { m_tipsOn = false; SaveTipsSettings(); Show(w, L"* Tips off.", cInfo); }
        else Show(w, m_tipsOn ? L"* Tips are on." : L"* Tips are off.", cInfo);
    }
    void CmdTip(CChatWnd* w, CString arg) {   // /tip <-ct> <name/N> [text]
        arg.Trim(); bool closeFlag = false, textFlag = false;
        while (arg.Left(1) == L"-") { CString sw = Word(arg); for (int i = 1; i < sw.GetLength(); i++) { if (sw[i] == L'c') closeFlag = true; else if (sw[i] == L't') textFlag = true; } arg.TrimLeft(); }
        CString sel = Word(arg), text = arg;
        auto it = m_tipQueue.end();
        double idxD; if (ParseNum(sel, idxD)) { int idx = (int)idxD; if (idx >= 1 && idx <= (int)m_tipQueue.size()) it = m_tipQueue.begin() + (idx - 1); }
        if (it == m_tipQueue.end()) for (auto i2 = m_tipQueue.begin(); i2 != m_tipQueue.end(); ++i2) if (i2->name.CompareNoCase(sel) == 0) { it = i2; break; }
        if (it == m_tipQueue.end()) { Show(w, L"* No such tip: " + sel, cPart); return; }
        if (closeFlag) { if (it == m_tipQueue.begin() && it->shownAt != 0) ClearTipBalloon(); m_tipQueue.erase(it); }
        else if (textFlag) it->text = text;
    }
    void CmdTitlebar(CChatWnd* w, CString arg) {   // /titlebar [@window] <text>: no @window given -> the main app titlebar; otherwise that custom window's own
        arg.Trim();
        if (arg.IsEmpty()) { Show(w, L"* Usage: /titlebar [@window] <text>", cPart); return; }
        CString first = arg; CString w1 = Word(first);
        if (!w1.IsEmpty() && w1[0] == L'@') {
            if (first.IsEmpty()) { Show(w, L"* Usage: /titlebar [@window] <text>", cPart); return; }
            CChatWnd* target = Find(nullptr, w1);
            if (!target) { Show(w, L"* No such window: " + w1, cPart); return; }
            target->SetWindowText(first);
            return;
        }
        SetWindowText(arg);
    }
    // ---- Sound playback: MCI handles wave/midi/mp3 uniformly, which is what mIRC's own /splay has always used under the hood ----
    static CString MciCmd(const CString& cmd) { wchar_t buf[512] = {}; return ::mciSendStringW(cmd, buf, _countof(buf), nullptr) == 0 ? CString(buf) : CString(); }
    static bool MciOk(const CString& cmd) { wchar_t buf[8] = {}; return ::mciSendStringW(cmd, buf, _countof(buf), nullptr) == 0; }
    static CString MciTry(const CString& cmd) {   // like MciOk, but returns the actual MCI error description on failure instead of just true/false -- lets /splay report *why* a sound wouldn't play (missing codec, bad path, etc.) instead of a bare "could not play"
        wchar_t buf[512] = {};
        MCIERROR err = ::mciSendStringW(cmd, buf, _countof(buf), nullptr);
        if (err == 0) return CString();
        wchar_t errBuf[256] = {}; ::mciGetErrorStringW(err, errBuf, _countof(errBuf));
        CString s; s.Format(L"MCI error %lu: %s", err, errBuf[0] ? errBuf : L"(no description)");
        return s;
    }
    static CString MciTypeForFile(const CString& file) {
        CString ext = file; int dot = ext.ReverseFind(L'.'); ext = dot >= 0 ? ext.Mid(dot + 1) : CString(); ext.MakeLower();
        if (ext == L"wav") return L"waveaudio";
        if (ext == L"mid" || ext == L"midi" || ext == L"rmi") return L"sequencer";
        if (ext == L"mp3" || ext == L"mp2") return L"mpegvideo";   // Windows' built-in mpegvideo MCI driver also handles audio-only MP3s
        return CString();
    }
    CString ResolveSoundPath(const CString& file) {
        if (file.Find(L':') >= 0 || file.Find(L'\\') >= 0 || file.Find(L'/') >= 0) return file;   // already has a path
        CString ext = file; int dot = ext.ReverseFind(L'.'); ext = dot >= 0 ? ext.Mid(dot + 1) : CString(); ext.MakeLower();
        CString dir = ext == L"wav" ? m_soundDirWave : (ext == L"mid" || ext == L"midi") ? m_soundDirMidi : (ext == L"mp3" || ext == L"mp2") ? m_soundDirMp3 : CString();
        if (!dir.IsEmpty()) {
            CString p = dir; if (p.Right(1) != L"\\" && p.Right(1) != L"/") p += L"\\";
            p += file;
            if (::GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) return p;
        }
        return file;
    }
    void CloseSoundChannel(SoundChannel& ch) {
        if (ch.open) MciOk(L"close " + ch.alias);
        ch.open = ch.playing = ch.paused = false; ch.curFile.Empty();
    }
    // Returns empty on success, or a human-readable reason on failure (missing file, no codec for this type, etc.)
    CString OpenAndPlaySound(SoundChannel& ch, const CString& file, int startMs = -1) {
        CloseSoundChannel(ch);
        CString type = MciTypeForFile(file);
        if (type.IsEmpty()) return L"Unsupported file type (only .wav, .mid/.midi, .mp3/.mp2 are recognized)";
        CString resolved = ResolveSoundPath(file);
        if (::GetFileAttributesW(resolved) == INVALID_FILE_ATTRIBUTES)
            return L"File not found: " + resolved + L" (relative filenames are resolved against the app's current working directory, not necessarily the .exe's own folder -- try a full path, or set the matching Sound Requests directory)";
        CString cmd; cmd.Format(L"open \"%s\" type %s alias %s", (LPCWSTR)resolved, (LPCWSTR)type, (LPCWSTR)ch.alias);
        CString err = MciTry(cmd);
        if (!err.IsEmpty()) return err;
        ch.open = true; ch.curFile = resolved;
        CString playCmd = L"play " + ch.alias;
        if (startMs >= 0) playCmd.AppendFormat(L" from %d", startMs);
        err = MciTry(playCmd);
        if (!err.IsEmpty()) { CloseSoundChannel(ch); return err; }
        ch.playing = true; ch.paused = false;
        return CString();
    }
    void TriggerSoundEvent(const CString& type, const CString& file) {   // this app has no formal "on EVENT" system, so the closest match to mIRC's "sound event" is: run an alias literally named "sound", if one is defined
        AliasDef* ad = FindAlias(L"sound");
        if (!ad) return;
        CChatWnd* w = nullptr; for (auto& kv : m_w) if (kv.second->m_name == L"*status*") { w = kv.second; break; }
        if (!w) w = m_w.empty() ? nullptr : m_w.begin()->second;
        if (w) RunAlias(w, *ad, type + L" " + file);
    }
    void SoundTick() {   // advances queues and fires the sound event, piggybacking on the regular ~500ms tick
        SoundChannel* chans[3] = { &m_waveChan, &m_midiChan, &m_mp3Chan };
        const wchar_t* typeNames[3] = { L"wave", L"midi", L"song" };
        for (int i = 0; i < 3; i++) {
            SoundChannel* ch = chans[i];
            if (!ch->open || ch->paused) continue;
            if (MciCmd(L"status " + ch->alias + L" mode").CompareNoCase(L"playing") == 0) continue;
            CString finishedFile = ch->curFile;
            CloseSoundChannel(*ch);
            TriggerSoundEvent(typeNames[i], finishedFile);
            if (!ch->queue.empty()) {
                auto next = ch->queue.front(); ch->queue.erase(ch->queue.begin());
                CString err = OpenAndPlaySound(*ch, next.first, next.second);
                if (!err.IsEmpty()) { CChatWnd* sw = m_w.empty() ? nullptr : m_w.begin()->second; for (auto& kv : m_w) if (kv.second->m_name == L"*status*") { sw = kv.second; break; } if (sw) Show(sw, L"* Could not play queued sound " + next.first + L": " + err, cPart); }
            }
        }
    }
    static bool GetMasterEndpoint(IAudioEndpointVolume** ppVol) {   // Core Audio: the modern (Vista+) way to read/set the system's own master volume and mute
        ::CoInitialize(nullptr);
        IMMDeviceEnumerator* enumerator = nullptr;
        if (FAILED(::CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator)) || !enumerator) return false;
        IMMDevice* device = nullptr;
        HRESULT hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        enumerator->Release();
        if (FAILED(hr) || !device) return false;
        hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)ppVol);
        device->Release();
        return SUCCEEDED(hr);
    }
    static void SetMasterVolumeNow(int v) { IAudioEndpointVolume* vol = nullptr; if (GetMasterEndpoint(&vol)) { vol->SetMasterVolumeLevelScalar((float)v / 65535.0f, nullptr); vol->Release(); } }
    static int GetMasterVolumeNow() { IAudioEndpointVolume* vol = nullptr; float f = 0; if (GetMasterEndpoint(&vol)) { vol->GetMasterVolumeLevelScalar(&f); vol->Release(); } return (int)(f * 65535.0f); }
    static void SetMasterMuteNow(bool m) { IAudioEndpointVolume* vol = nullptr; if (GetMasterEndpoint(&vol)) { vol->SetMute(m, nullptr); vol->Release(); } }
    static bool GetMasterMuteNow() { IAudioEndpointVolume* vol = nullptr; BOOL m = FALSE; if (GetMasterEndpoint(&vol)) { vol->GetMute(&m); vol->Release(); } return m != FALSE; }
    void CmdSplay(CChatWnd* w, CString arg) {
        arg.Trim();
        bool wFlag = false, mFlag = false, pFlag = false, qFlag = false, cFlag = false;
        while (arg.Left(1) == L"-") {
            CString sw = Word(arg);
            for (int i = 1; i < sw.GetLength(); i++) { wchar_t c = sw[i]; if (c == L'w') wFlag = true; else if (c == L'm') mFlag = true; else if (c == L'p') pFlag = true; else if (c == L'q') qFlag = true; else if (c == L'c') cFlag = true; }
            arg.TrimLeft();
        }
        if (cFlag) {
            bool any = !wFlag && !mFlag && !pFlag;
            if (wFlag || any) m_waveChan.queue.clear(); if (mFlag || any) m_midiChan.queue.clear(); if (pFlag || any) m_mp3Chan.queue.clear();
            if (arg.IsEmpty()) { Show(w, L"* Sound queue cleared.", cInfo); return; }
        }
        CString first = arg; CString w1 = Word(first); CString w1l = w1; w1l.MakeLower();
        auto targets = [&]() { std::vector<SoundChannel*> t; if (wFlag) t.push_back(&m_waveChan); if (mFlag) t.push_back(&m_midiChan); if (pFlag) t.push_back(&m_mp3Chan); if (t.empty()) t = { &m_waveChan, &m_midiChan, &m_mp3Chan }; return t; };
        if (w1l == L"stop" || w1l == L"pause" || w1l == L"resume" || w1l == L"skip") {
            for (auto* ch : targets()) {
                if (!ch->open) continue;
                if (w1l == L"stop") CloseSoundChannel(*ch);
                else if (w1l == L"pause") { MciOk(L"pause " + ch->alias); ch->paused = true; }
                else if (w1l == L"resume") { MciOk(L"resume " + ch->alias); ch->paused = false; }
                else if (w1l == L"skip") {
                    CloseSoundChannel(*ch);
                    if (!ch->queue.empty()) { auto next = ch->queue.front(); ch->queue.erase(ch->queue.begin()); CString err = OpenAndPlaySound(*ch, next.first, next.second); if (!err.IsEmpty()) Show(w, L"* Could not play " + next.first + L": " + err, cPart); }
                }
            }
            return;
        }
        if (w1l == L"seek") { if (m_mp3Chan.open) MciOk(L"play " + m_mp3Chan.alias + L" from " + first); return; }   // seeks the mp3 channel specifically, matching the spec's own example
        if (arg.IsEmpty()) { Show(w, L"* Usage: /splay -cwmpq [filename|stop|pause|resume|seek|skip] [pos]", cPart); return; }
        CString fname = w1; CString posStr = first; posStr.Trim();
        int startMs = posStr.IsEmpty() ? -1 : _wtoi(posStr);
        CString type = MciTypeForFile(fname);
        if (type.IsEmpty()) { Show(w, L"* Unsupported sound file type: " + fname, cPart); return; }
        SoundChannel* ch = wFlag ? &m_waveChan : mFlag ? &m_midiChan : pFlag ? &m_mp3Chan : type == L"waveaudio" ? &m_waveChan : type == L"sequencer" ? &m_midiChan : &m_mp3Chan;
        if (qFlag && ch->open) { ch->queue.push_back({ fname, startMs }); return; }
        CString err = OpenAndPlaySound(*ch, fname, startMs);
        if (!err.IsEmpty()) Show(w, L"* Could not play " + fname + L": " + err, cPart);
    }
    void CmdVol(CChatWnd* w, CString arg) {
        arg.Trim();
        bool wFlag = false, mFlag = false, pFlag = false, vFlag = false; int muteN = -1;
        while (arg.Left(1) == L"-") {
            CString sw = Word(arg);
            for (int i = 1; i < sw.GetLength(); i++) {
                wchar_t c = sw[i];
                if (c == L'w') wFlag = true; else if (c == L'm') mFlag = true; else if (c == L'p') pFlag = true; else if (c == L'v') vFlag = true;
                else if (c == L'u') { CString digs; while (i + 1 < sw.GetLength() && iswdigit(sw[i + 1])) digs += sw[++i]; muteN = digs.IsEmpty() ? 0 : _wtoi(digs); }
            }
            arg.TrimLeft();
        }
        if (muteN >= 0 && vFlag) SetMasterMuteNow(muteN == 1);   // per-stream mute for wave/midi/mp3 isn't separately implemented -- neither waveOutSetVolume nor MCI expose a clean per-stream mute flag the way the system mixer does; only -v's master mute is real here
        if (!arg.IsEmpty()) {
            int v = _wtoi(arg); if (v < 0) v = 0; if (v > 65535) v = 65535;
            bool any = !wFlag && !mFlag && !pFlag && !vFlag;
            if (vFlag) SetMasterVolumeNow(v);
            if (wFlag || any) { DWORD vol = MAKELONG((WORD)v, (WORD)v); ::waveOutSetVolume(nullptr, vol); }
            if (mFlag) MciOk(L"setaudio " + m_midiChan.alias + L" volume to " + CString(std::to_wstring(v * 1000 / 65535).c_str()));   // MCI volume is 0-1000; only takes effect while that channel is open/playing
            if (pFlag) MciOk(L"setaudio " + m_mp3Chan.alias + L" volume to " + CString(std::to_wstring(v * 1000 / 65535).c_str()));
        }
        Show(w, L"* Volume updated.", cInfo);
    }
    // ---- ID3v1 tag reading for $sound(file.mp3) -- the simple, fixed-size 128-byte trailer format only; full ID3v2
    // (.id3/.tag/.tags) and MPEG-header-derived properties (bitrate, vbr, sample rate, mode, version, copyright,
    // private, crc) are NOT implemented -- those need actual frame/tag-structure parsing well beyond this format.
    struct Id3v1Tag { CString title, artist, album, year, comment, genre, track; };
    static CString Id3Str(const char* p, int len) {
        CStringA a(p, len); int end = a.GetLength(); while (end > 0 && (a[end - 1] == 0 || a[end - 1] == ' ')) end--;
        return CString(a.Left(end));
    }
    static bool ReadId3v1(const CString& path, Id3v1Tag& tag) {
        CFile f; if (!f.Open(path, CFile::modeRead | CFile::shareDenyNone)) return false;
        if (f.GetLength() < 128) return false;
        f.Seek(-128, CFile::end);
        char buf[128]; if (f.Read(buf, 128) != 128 || memcmp(buf, "TAG", 3) != 0) return false;
        tag.title = Id3Str(buf + 3, 30); tag.artist = Id3Str(buf + 33, 30); tag.album = Id3Str(buf + 63, 30); tag.year = Id3Str(buf + 93, 4);
        bool v11 = buf[125] == 0 && buf[126] != 0;   // ID3v1.1: byte 125 is a zero separator, byte 126 is the track number, shortening the comment by 2 bytes
        tag.comment = Id3Str(buf + 97, v11 ? 28 : 30);
        tag.track = v11 ? CString(std::to_wstring((unsigned char)buf[126]).c_str()) : CString();
        tag.genre.Format(L"%d", (int)(unsigned char)buf[127]);   // reported as the raw numeric genre id -- not mapped to the standard genre-name table
        return true;
    }
    void LoadIdentd() {
        CWinApp* a = AfxGetApp();
        m_identdEnabled = a->GetProfileInt(L"Identd", L"enabled", 0) != 0;
        m_identdShowReq = a->GetProfileInt(L"Identd", L"showReq", 1) != 0;
        m_identdOnlyConnecting = a->GetProfileInt(L"Identd", L"onlyConnecting", 0) != 0;
        m_identdUseEmail = a->GetProfileInt(L"Identd", L"useEmail", 0) != 0;
        m_identdUserId = a->GetProfileString(L"Identd", L"userId", L"user");
        m_identdSystem = a->GetProfileString(L"Identd", L"system", L"UNIX");
        m_identdPort = a->GetProfileInt(L"Identd", L"port", 113);
    }
    void SaveIdentd() {
        CWinApp* a = AfxGetApp();
        a->WriteProfileInt(L"Identd", L"enabled", m_identdEnabled ? 1 : 0);
        a->WriteProfileInt(L"Identd", L"showReq", m_identdShowReq ? 1 : 0);
        a->WriteProfileInt(L"Identd", L"onlyConnecting", m_identdOnlyConnecting ? 1 : 0);
        a->WriteProfileInt(L"Identd", L"useEmail", m_identdUseEmail ? 1 : 0);
        a->WriteProfileString(L"Identd", L"userId", m_identdUserId);
        a->WriteProfileString(L"Identd", L"system", m_identdSystem);
        a->WriteProfileInt(L"Identd", L"port", m_identdPort);
    }
    bool IdentdRunning() const { return m_identdState && m_identdState->running; }
    void StartIdentd(Net* triggerNet = nullptr) {
        if (IdentdRunning()) return;
        m_identdTriggerNet = triggerNet;
        m_identdState = std::make_shared<IdentdState>();
        CString uid = m_identdUseEmail && triggerNet ? triggerNet->o.user : m_identdUserId;   // "from email address": no email field exists here, so the connection's Username (ident) field is the closest analog
        m_identdState->userId = (LPCWSTR)uid; m_identdState->system = (LPCWSTR)m_identdSystem;
        m_identdState->port = m_identdPort; m_identdState->hwnd = m_hWnd;
        auto* passState = new std::shared_ptr<IdentdState>(m_identdState);
        uintptr_t th = _beginthreadex(nullptr, 0, IdentdThreadProc, passState, 0, nullptr);
        if (th) CloseHandle((HANDLE)th); else { delete passState; m_identdState.reset(); }
        if (m_identdOnlyConnecting) m_identdAutoStopAt = GetTickCount64() + 60000;   // stop after a minute even if nothing ever queries it
    }
    void StopIdentd() {
        if (m_identdState) m_identdState->stop = true;
        m_identdState.reset(); m_identdTriggerNet = nullptr; m_identdAutoStopAt = 0;
    }
    void CmdIdentd(CChatWnd* w, CString arg) {
        arg.Trim();
        if (arg.IsEmpty()) { Show(w, IdentdRunning() ? CString(L"* Identd server is running.") : CString(L"* Identd server is off."), cInfo); return; }
        CString mode = Word(arg); CString modeL = mode; modeL.MakeLower();
        if (modeL == L"on") { m_identdEnabled = true; if (!arg.IsEmpty()) m_identdUserId = arg; SaveIdentd(); if (!m_identdOnlyConnecting) StartIdentd(); Show(w, L"* Identd server on" + (arg.IsEmpty() ? CString() : L", user id: " + arg) + L".", cInfo); }
        else if (modeL == L"off") { m_identdEnabled = false; SaveIdentd(); StopIdentd(); Show(w, L"* Identd server off.", cInfo); }
        else { Show(w, L"* Usage: /identd [on|off] [userid]", cPart); return; }
    }
    afx_msg LRESULT OnIdentdRequest(WPARAM, LPARAM lp) {
        std::unique_ptr<std::pair<std::wstring, std::wstring>> notice((std::pair<std::wstring, std::wstring>*)lp);
        if (m_identdShowReq) {
            CString peer = notice->first.c_str(), reply = notice->second.c_str(); reply.TrimRight(L"\r\n");
            CChatWnd* sw = nullptr;
            if (m_identdTriggerNet) sw = Status(m_identdTriggerNet);
            else for (auto& np : m_nets) if (np->conn) { sw = Status(np.get()); break; }
            if (sw) Show(sw, L"* Identd request from " + peer + L": " + reply, cInfo);
        }
        if (m_identdOnlyConnecting) StopIdentd();
        return 0;
    }
    void OnIdentdDialog() {
        CIdentdDlg dlg(m_identdEnabled, m_identdUserId, m_identdSystem, m_identdPort, m_identdShowReq, m_identdOnlyConnecting, m_identdUseEmail, this);
        if (dlg.DoModal() != IDOK) return;
        bool wasContinuous = m_identdEnabled && !m_identdOnlyConnecting;
        m_identdEnabled = dlg.enabled; m_identdUserId = dlg.userId; m_identdSystem = dlg.system; m_identdPort = dlg.port;
        m_identdShowReq = dlg.showReq; m_identdOnlyConnecting = dlg.onlyConnecting; m_identdUseEmail = dlg.useEmail;
        SaveIdentd();
        bool wantContinuous = m_identdEnabled && !m_identdOnlyConnecting;
        if (wasContinuous && !wantContinuous) StopIdentd();
        else if (wantContinuous) { StopIdentd(); StartIdentd(); }   // restart so a changed port/userid/system actually takes effect
    }
    void OnOnlineTimerDialog() {
        COnlineTimerDlg dlg(this);
        dlg.enabled = m_otEnabled; dlg.showTotal = m_otShowTotal;
        dlg.getCurrent = [this] { return OtCurrentSeconds(); };
        dlg.getTotal = [this] { return OtTotalSeconds(); };
        dlg.getCurrentResetDate = [this] { return OtFormatDate(m_otSessionResetTime); };
        dlg.getTotalResetDate = [this] { return OtFormatDate(m_otTotalResetTime); };
        dlg.onResetCurrent = [this] { OtResetCurrent(); };
        dlg.onResetTotal = [this] { OtResetTotal(); };
        if (dlg.DoModal() != IDOK) return;
        m_otEnabled = dlg.enabled; m_otShowTotal = dlg.showTotal;
        SaveOnlineTimer();
        UpdateOnlineTimer();
    }
    void WriteLog(Net* net, const CString& winName, const CString& rawLine) {
        if (!m_logEnabled || m_logFolder.IsEmpty()) return;
        CString folder = m_logFolder; if (folder.Right(1) != L"\\") folder += L"\\";
        CString netTag = (net && !net->tag.IsEmpty()) ? net->tag : (net ? net->o.host : CString(L"unknown"));
        CString win = winName == L"*status*" ? CString(L"status") : winName;
        CString path = folder + SanitizeFileName(netTag) + L"_" + SanitizeFileName(win) + L".log";
        CString line = FormatTimestamp(m_tsLogFmt) + L" " + rawLine;   // the log's own timestamp format, independent of what's shown on screen (see /timestamp -g); the space is always added regardless of the format string
        FILE* f = nullptr;
        if (_wfopen_s(&f, path, L"a, ccs=UTF-8") == 0 && f) { fwprintf(f, L"%s\n", (LPCWSTR)line); fclose(f); }
    }

    // ---- /play: queues a text file's lines to be sent out one at a time on a timer (see PlayTick) ----
    static std::vector<CString> PlayTokenize(const CString& s) {   // splits on spaces, honoring "quoted phrases" as one token
        std::vector<CString> out; CString cur = s; cur.TrimLeft();
        while (!cur.IsEmpty()) {
            CString tok;
            if (cur[0] == L'"') { int e = cur.Find(L'"', 1); if (e > 0) { tok = cur.Mid(1, e - 1); cur = cur.Mid(e + 1); } else { tok = cur.Mid(1); cur.Empty(); } }
            else { int sp = cur.Find(L' '); if (sp < 0) { tok = cur; cur.Empty(); } else { tok = cur.Left(sp); cur = cur.Mid(sp + 1); } }
            cur.TrimLeft(); if (!tok.IsEmpty()) out.push_back(tok);
        }
        return out;
    }
    static bool IsAllDigits(const CString& s) { if (s.IsEmpty()) return false; for (int i = 0; i < s.GetLength(); i++) if (!iswdigit(s[i])) return false; return true; }
    static std::vector<CString> ReadTextLines(const CString& path) {
        std::vector<CString> lines; FILE* f = nullptr;
        if (_wfopen_s(&f, path, L"r, ccs=UTF-8") != 0 || !f) return lines;
        wchar_t buf[4096];
        while (fgetws(buf, 4096, f)) { CString l = buf; l.TrimRight(L"\r\n"); lines.push_back(l); }
        fclose(f); return lines;
    }
    static bool IsSectionHeader(const CString& raw, CString* name) {   // a trimmed line reading exactly "[name]"
        CString t = raw; t.Trim();
        if (t.GetLength() < 3 || t[0] != L'[' || t[t.GetLength() - 1] != L']') return false;
        if (name) *name = t.Mid(1, t.GetLength() - 2);
        return true;
    }
    void CmdPlay(CChatWnd* w, CString arg) {
        Net* net = w->net; arg.Trim();
        if (arg.CompareNoCase(L"stop") == 0) { m_playQueue.clear(); StopPlayTimer(); Show(w, L"* Playback stopped and queue cleared.", cInfo); return; }
        if (arg.IsEmpty()) { Show(w, L"* Usage: /play [-aescpbnrx q# m# l# f# t<topic>] [alias] [channel/nick] <filename> [delay]  or  /play stop  (see /playctrl to manage the queue)", cPart); return; }
        bool aFlag = false, eFlag = false, sFlag = false, cFlag = false, pFlag = false, bFlag = false, nFlag = false, rFlag = false, xFlag = false;
        int qMax = -1, mMax = -1, lLine = -1, fFrom = -1; CString topic;
        if (arg[0] == L'-') {
            int i = 1; for (; i < arg.GetLength() && arg[i] != L' '; i++) {
                wchar_t c = arg[i];
                if (c == L'a') aFlag = true; else if (c == L'e') eFlag = true; else if (c == L's') sFlag = true;
                else if (c == L'c') cFlag = true; else if (c == L'p') pFlag = true; else if (c == L'b') bFlag = true;
                else if (c == L'n') nFlag = true; else if (c == L'r') rFlag = true; else if (c == L'x') xFlag = true;
                else if (c == L'q' || c == L'm' || c == L'l' || c == L'f') {
                    CString digs; while (i + 1 < arg.GetLength() && iswdigit(arg[i + 1])) digs += arg[++i];
                    int v = digs.IsEmpty() ? 0 : _wtoi(digs);
                    if (c == L'q') qMax = v; else if (c == L'm') mMax = v; else if (c == L'l') lLine = v; else fFrom = v;
                } else if (c == L't') {   // -t consumes the rest of this token as the topic name, e.g. "-thelp1" -> topic "help1"
                    int sp = arg.Find(L' ', i);
                    topic = arg.Mid(i + 1, (sp < 0 ? arg.GetLength() : sp) - i - 1);
                    i = (sp < 0 ? arg.GetLength() : sp) - 1;   // -1 because the for loop's own i++ runs next; leaves i pointing at the space (or end)
                }
            }
            arg = arg.Mid(i); arg.TrimLeft();
        }
        std::vector<CString> tok = PlayTokenize(arg);
        CString aliasName; if (aFlag && !tok.empty()) { aliasName = tok.front(); tok.erase(tok.begin()); }
        int delay = 1000;
        if (!tok.empty() && IsAllDigits(tok.back()) && (bFlag ? tok.size() >= 1 : tok.size() >= 2)) { delay = _wtoi(tok.back()); tok.pop_back(); }
        CString target, fname;
        if (bFlag) { if (!tok.empty()) target = tok.front(); }
        else {
            if (tok.empty()) { Show(w, L"* /play needs a filename.", cPart); return; }
            fname = tok.back(); tok.pop_back();
            if (!tok.empty()) target = tok.front();
        }
        if (target.IsEmpty()) target = (w->m_name != L"*status*") ? w->m_name : CString();
        if (target.IsEmpty() || target == L"*status*") { Show(w, L"* /play needs a channel or nick (the current window is Status).", cPart); return; }
        if (!sFlag && (!net || !net->conn)) { Show(w, L"* Not connected. Use -s to play to a window while offline.", cPart); return; }
        PlayItem it; it.net = net; it.target = target; it.alias = aliasName; it.topic = topic;
        it.delay = delay; it.echo = eFlag; it.asCmd = cFlag; it.notice = nFlag;
        if (bFlag) {
            CString clip; if (OpenClipboard()) { HANDLE h = GetClipboardData(CF_UNICODETEXT); if (h) clip = (LPCWSTR)GlobalLock(h); if (h) GlobalUnlock(h); CloseClipboard(); }
            if (clip.IsEmpty()) { Show(w, L"* Clipboard is empty or isn't text.", cPart); return; }
            fname = IniPath(L"playclip.txt"); FILE* f = nullptr;
            if (_wfopen_s(&f, fname, L"w, ccs=UTF-8") == 0 && f) { fwprintf(f, L"%s", (LPCWSTR)clip); fclose(f); }
            it.clipTemp = true;
        }
        it.fname = fname;
        std::vector<CString> all = ReadTextLines(fname);
        if (all.empty()) { Show(w, L"* Couldn't read (or empty): " + fname, cPart); return; }
        bool hadCountHint = !all.empty() && IsAllDigits(CString(all[0]).Trim(L" \t"));
        if (!topic.IsEmpty()) {
            size_t start = SIZE_MAX; CString hdr;
            for (size_t i = 0; i < all.size(); i++) if (IsSectionHeader(all[i], &hdr) && hdr.CompareNoCase(topic) == 0) { start = i + 1; break; }
            if (start == SIZE_MAX) { Show(w, L"* Topic [" + topic + L"] not found in " + fname, cPart); return; }
            for (size_t i = start; i < all.size() && !IsSectionHeader(all[i], nullptr); i++) it.lines.push_back(all[i]);
        } else {
            size_t base = (rFlag || lLine > 0 || fFrom > 0) && !xFlag && hadCountHint ? 1 : 0;   // the optional line-count hint on line 1, per mIRC's spec
            if (rFlag) {
                std::vector<CString> nonEmpty; for (size_t i = base; i < all.size(); i++) if (!CString(all[i]).Trim().IsEmpty()) nonEmpty.push_back(all[i]);
                if (!nonEmpty.empty()) it.lines.push_back(nonEmpty[rand() % nonEmpty.size()]);
            } else if (lLine > 0) { size_t idx = base + lLine - 1; if (idx < all.size()) it.lines.push_back(all[idx]); }
            else if (fFrom > 0) { for (size_t i = base + fFrom - 1; i < all.size(); i++) it.lines.push_back(all[i]); }
            else for (size_t i = 0; i < all.size(); i++) it.lines.push_back(all[i]);
        }
        if (it.lines.empty()) { Show(w, L"* Nothing to play from " + fname, cPart); return; }
        if (qMax >= 0 && (int)m_playQueue.size() >= qMax) { CString msg; msg.Format(L"* Play queue is full (-q%d).", qMax); Show(w, msg, cPart); return; }
        if (mMax >= 0) { int have = 0; for (auto& q : m_playQueue) if (q.target.CompareNoCase(target) == 0) have++; if (have >= mMax) { Show(w, L"* That target already has enough queued (-m).", cPart); return; } }
        if (pFlag && !m_playQueue.empty()) m_playQueue.insert(m_playQueue.begin(), it); else m_playQueue.push_back(it);
        CString msg; msg.Format(L"* Queued %s -> %s (%d line(s), %dms)", (LPCWSTR)fname, (LPCWSTR)target, (int)it.lines.size(), delay);
        Show(w, msg, cInfo);
        StartPlayTimer();
    }
    void StartPlayTimer() { if (!m_playTimerId && !m_playQueue.empty()) m_playTimerId = SetTimer(2001, 50, nullptr); }
    void StopPlayTimer() { if (m_playTimerId) { KillTimer(m_playTimerId); m_playTimerId = 0; } }
    void PlayTick() {   // called every 50ms; the head item only actually sends once its own dueAt has elapsed
        if (m_playQueue.empty()) { StopPlayTimer(); return; }
        PlayItem& it = m_playQueue.front();
        ULONGLONG now = GetTickCount64();
        if (it.dueAt == 0) it.dueAt = now;
        if (now < it.dueAt) return;
        if (it.pos >= it.lines.size()) {
            if (it.clipTemp) ::DeleteFileW(it.fname);
            m_playQueue.erase(m_playQueue.begin());
            if (m_playQueue.empty()) StopPlayTimer();
            return;
        }
        CString line = it.lines[it.pos++]; m_pnick = it.target;
        CChatWnd* w = Find(it.net, it.target); if (!w && it.net) w = Open(it.net, it.target, IsChan(it.target));   // -s only controls whether a connection is required to start playing at all (checked in CmdPlay); the target window itself always opens normally
        if (!line.IsEmpty()) {
            if (it.asCmd) { if (w) Dispatch(w, line); }
            else if (!it.alias.IsEmpty()) { if (w) { if (AliasDef* ad = FindAlias(it.alias)) RunAlias(w, *ad, it.target + L" " + line); } }
            else if (it.notice) { if (it.net && it.net->conn) Send(it.net, L"NOTICE " + it.target + L" :" + line); if (it.echo && w) Show(w, L"-> -" + it.target + L"- " + line, cNotice); }
            else { if (it.net && it.net->conn) Say(it.net, it.target, line); }
        }
        it.dueAt = now + (ULONGLONG)it.delay;
    }
    void CmdPlayCtrl(CChatWnd* w) {
        CPlayCtrlDlg dlg(m_playQueue, this);
        dlg.DoModal();
        if (m_playQueue.empty()) StopPlayTimer();
    }

    // ---- /window: create/manipulate a custom @window. A large chunk of mIRC's own switch list has no equivalent in
    // this client (desktop windows, treebar, side-listbox, progress bar, picture windows, tab stops, icons,
    // fullscreen) and is accepted-but-ignored so a script's switch string doesn't error out; see CmdWindow's inline
    // notes for exactly what each switch does here. ----
    void CmdWindow(CChatWnd* w, CString arg) {
        CString a = arg; a.Trim();
        if (a.IsEmpty()) { Show(w, L"* Usage: /window [switches] <@name> [x y [w h]] [/command] [popup.txt] [font [size]]", cPart); return; }
        bool aFlag = false, cFlag = false, hFlag = false, eFlag = false, lFlag = false, CFlag = false, sFlagSw = false;
        bool nFlagSw = false, rFlagSw = false, xFlagSw = false;
        while (!a.IsEmpty() && (a[0] == L'-' || a[0] == L'+')) {
            CString swTok = Word(a);
            if (swTok[0] == L'-') {
                for (int i = 1; i < swTok.GetLength(); i++) {
                    wchar_t c = swTok[i];
                    if (c == L'a') aFlag = true; else if (c == L'c') cFlag = true; else if (c == L'h') hFlag = true;
                    else if (c == L'e') { eFlag = true; while (i + 1 < swTok.GetLength() && iswdigit(swTok[i + 1])) i++; }
                    else if (c == L'l') { lFlag = true; while (i + 1 < swTok.GetLength() && iswdigit(swTok[i + 1])) i++; }
                    else if (c == L'n') { nFlagSw = true; while (i + 1 < swTok.GetLength() && iswdigit(swTok[i + 1])) i++; }
                    else if (c == L'r') rFlagSw = true; else if (c == L'x') xFlagSw = true; else if (c == L'C') CFlag = true;
                    else if (c == L's') sFlagSw = true;
                    else if (c == L't') while (i + 1 < swTok.GetLength() && (iswdigit(swTok[i + 1]) || swTok[i + 1] == L',')) i++;   // -tN,..,N (tab stops): parsed past, not applied
                    // everything else (b B d D f g[N] G H i j[N] k[N] m M o p q R u v w[N] z, and +switches) is
                    // accepted for compatibility but has no effect: no desktop-window mode, treebar, side-listbox,
                    // progress bar, picture windows, custom border styles, or icons in this client.
                }
            }
            a.TrimLeft();
        }
        CString name = Word(a);
        if (name.IsEmpty() || name[0] != L'@') { Show(w, L"* /window: the name must start with @, e.g. @test", cPart); return; }
        std::vector<CString> tok = PlayTokenize(a);
        auto isNum = [](const CString& s) { if (s.IsEmpty()) return false; int st = s[0] == L'-' ? 1 : 0; if (st >= s.GetLength()) return false; for (int i = st; i < s.GetLength(); i++) if (!iswdigit(s[i])) return false; return true; };
        size_t ti = 0; int px = -1, py = -1, pw = -1, ph = -1;
        if (ti + 1 < tok.size() && isNum(tok[ti]) && isNum(tok[ti + 1])) {
            px = _wtoi(tok[ti]); py = _wtoi(tok[ti + 1]); ti += 2;
            if (ti + 1 < tok.size() && isNum(tok[ti]) && isNum(tok[ti + 1])) { pw = _wtoi(tok[ti]); ph = _wtoi(tok[ti + 1]); ti += 2; }
        }
        CString defCmd, popupFile, fontName; int fontSize = -1;
        if (ti < tok.size() && tok[ti][0] == L'/') { defCmd = tok[ti]; ti++; }
        if (ti < tok.size() && tok[ti].Find(L'.') >= 0) { popupFile = tok[ti]; ti++; }
        if (ti < tok.size()) { fontName = tok[ti]; ti++; if (ti < tok.size() && isNum(tok[ti])) { fontSize = _wtoi(tok[ti]); ti++; } }

        CChatWnd* cw = Find(nullptr, name);
        if (cFlag) { if (cw) cw->DestroyWindow(); return; }
        bool creating = !cw;
        if (!cw) cw = OpenCustomWindow(name, hFlag);
        if (eFlag) cw->m_hasEdit = true;
        if (lFlag) cw->m_cwListMode = true;
        if (sFlagSw) { cw->m_cwSort = true; CwSort(cw); }
        if (!defCmd.IsEmpty()) cw->m_cwDefCmd = defCmd;
        if (!popupFile.IsEmpty()) cw->m_cwPopup = ReadTextLines(popupFile);
        if (!fontName.IsEmpty()) { LOGFONT lf; MakeFont(lf, fontName, fontSize > 0 ? fontSize : 10, false, false); cw->ApplyFont(lf); }
        if (CFlag) {   // center on the primary monitor with a sensible default size, unless the caller also gave one
            CRect scr; ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &scr, 0);
            if (pw < 0) pw = 400; if (ph < 0) ph = 300;
            if (px < 0) px = scr.left + ((scr.Width() - pw) / 2); if (py < 0) py = scr.top + ((scr.Height() - ph) / 2);
        }
        if (px >= 0 || py >= 0 || pw >= 0 || ph >= 0) {
            CRect cur; cw->GetWindowRect(cur); ::MapWindowPoints(nullptr, m_hWndMDIClient, (LPPOINT)&cur, 2);
            cw->MoveWindow(px >= 0 ? px : cur.left, py >= 0 ? py : cur.top, pw >= 0 ? pw : cur.Width(), ph >= 0 ? ph : cur.Height());
        }
        if (hFlag && !creating) cw->ShowWindow(SW_HIDE);
        if (nFlagSw) cw->ShowWindow(SW_MINIMIZE);
        if (rFlagSw) cw->ShowWindow(SW_RESTORE);
        if (xFlagSw) cw->ShowWindow(SW_MAXIMIZE);
        if (aFlag) Activate(cw);
    }
    static void CwSort(CChatWnd* cw) {   // -s: keeps m_cwLines (and the parallel color array) sorted together
        std::vector<size_t> order(cw->m_cwLines.size()); for (size_t i = 0; i < order.size(); i++) order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return cw->m_cwLines[a].CompareNoCase(cw->m_cwLines[b]) < 0; });
        std::vector<CString> nl; std::vector<COLORREF> nc;
        for (size_t i : order) { nl.push_back(cw->m_cwLines[i]); nc.push_back(i < cw->m_cwColors.size() ? cw->m_cwColors[i] : cText); }
        cw->m_cwLines = nl; cw->m_cwColors = nc; cw->CwRebuild();
    }
    // ---- /aline /cline /dline /iline /rline /sline: shared parser, since they all start the same way ----
    // op: 'a' append, 'c' recolor, 'd' delete, 'i' insert, 'r' replace, 's' select
    void CmdCwLine(CChatWnd* w, CString arg, wchar_t op) {
        CString a = arg; a.TrimLeft();
        bool selAdd = false, selClear = false; int colorNum = -1;
        while (a.Left(1) == L"-") {   // -s/-a (selection mode), -h, -p, -r, -i[N], -n, -m, -l: accepted, only -s/-a/-i are meaningfully used here
            CString swTok = Word(a);
            for (int i = 1; i < swTok.GetLength(); i++) { wchar_t c = swTok[i]; if (c == L's') selClear = true; else if (c == L'a') selAdd = true; }
            a.TrimLeft();
        }
        CString first = a; CString maybeColor = Word(first);
        if (IsAllDigits(maybeColor)) { colorNum = _wtoi(maybeColor); a = first; }
        CString name = Word(a);
        CChatWnd* cw = Find(nullptr, name);
        if (!cw || !cw->m_custom) { Show(w, L"* No such window: " + name, cPart); return; }
        COLORREF col = colorNum >= 0 ? MircColor(colorNum) : cText;
        if (op == L'a') {   // /aline [c] <@name> <text>
            cw->m_cwLines.push_back(a); cw->m_cwColors.push_back(col);
            if (cw->m_cwSort) CwSort(cw); else cw->CwRebuild();
            if (selClear || selAdd) cw->m_cwSelectedLine = (int)cw->m_cwLines.size(), cw->CwSelectLine(cw->m_cwSelectedLine);
            return;
        }
        int n1 = 0, n2 = 0; CString rest = a; CString nTok = Word(rest);
        int dash = nTok.Find(L'-');
        if (dash > 0) { n1 = _wtoi(nTok.Left(dash)); n2 = _wtoi(nTok.Mid(dash + 1)); } else { n1 = n2 = _wtoi(nTok); }
        if (n1 < 1 || n1 > (int)cw->m_cwLines.size()) { Show(w, L"* No such line.", cPart); return; }
        if (op == L'c') { if ((size_t)n1 > cw->m_cwColors.size()) cw->m_cwColors.resize(n1, cText); cw->m_cwColors[n1 - 1] = col; cw->CwRebuild(); }
        else if (op == L'd') {
            n2 = (std::min)(n2 < n1 ? n1 : n2, (int)cw->m_cwLines.size());
            cw->m_cwLines.erase(cw->m_cwLines.begin() + (n1 - 1), cw->m_cwLines.begin() + n2);
            if ((size_t)n2 <= cw->m_cwColors.size()) cw->m_cwColors.erase(cw->m_cwColors.begin() + (n1 - 1), cw->m_cwColors.begin() + n2);
            cw->CwRebuild();
        } else if (op == L'i') {
            cw->m_cwLines.insert(cw->m_cwLines.begin() + (n1 - 1), rest);
            cw->m_cwColors.insert(cw->m_cwColors.begin() + (std::min)((size_t)(n1 - 1), cw->m_cwColors.size()), col);
            cw->CwRebuild();
        } else if (op == L'r') { cw->m_cwLines[n1 - 1] = rest; if (colorNum >= 0 && (size_t)n1 <= cw->m_cwColors.size()) cw->m_cwColors[n1 - 1] = col; cw->CwRebuild(); }   // only touches the color if one was actually given -- a plain text replace shouldn't silently reset it
        else if (op == L's') { cw->m_cwSelectedLine = n1; cw->CwSelectLine(n1); }
    }
    void CmdRenwin(CChatWnd* w, CString arg) {   // /renwin <@oldname> <@newname> [topic]
        CString a = arg; CString oldName = Word(a), newName = Word(a);
        CChatWnd* cw = Find(nullptr, oldName);
        if (!cw || !cw->m_custom) { Show(w, L"* No such window: " + oldName, cPart); return; }
        if (newName.IsEmpty() || newName[0] != L'@') { Show(w, L"* /renwin: the new name must start with @.", cPart); return; }
        m_w.erase(Key(nullptr, cw->m_name)); cw->m_name = newName; m_w[Key(nullptr, newName)] = cw;
        cw->SetWindowText(newName + (a.IsEmpty() ? CString() : L" " + a));
    }

    // ---- /timer / /timers: scheduled, optionally repeating commands ----
    // Forces $!identifier (or $!identifier(args)) references to evaluate once, right now, substituting the literal
    // result into the stored command; every other identifier is left as-is, to be evaluated fresh by the normal
    // script engine each time the timer actually fires.
    CString TimerPreEval(CChatWnd* w, const CString& cmd) {
        CString out; int i = 0, n = cmd.GetLength();
        while (i < n) {
            if (cmd[i] == L'$' && i + 1 < n && cmd[i + 1] == L'!') {
                int j = i + 2; while (j < n && (iswalnum(cmd[j]) || cmd[j] == L'_')) j++;
                CString ident = cmd.Mid(i + 2, j - (i + 2));
                CString whole = L"$" + ident;
                if (j < n && cmd[j] == L'(') { int depth = 1, k = j + 1; while (k < n && depth > 0) { if (cmd[k] == L'(') depth++; else if (cmd[k] == L')') depth--; k++; } whole = cmd.Mid(i + 1, k - (i + 1)); j = k; }
                out += EvalIds(w, whole, CString()); m_halt = false; i = j;
            } else { out += cmd[i]; i++; }
        }
        return out;
    }
    CString TimerId(const TimerInfo& t) { return t.name; }
    void ShowTimerStatus(CChatWnd* w, const TimerInfo& t) {
        CString reps = t.totalReps == 0 ? CString(L"*") : CString(std::to_wstring(t.repsLeft).c_str()) + L"/" + CString(std::to_wstring(t.totalReps).c_str());
        CString s; s.Format(L"* Timer%s: %s %s%s, interval %g%s -> %s", (LPCWSTR)t.name, (LPCWSTR)reps,
            t.paused ? L"[paused] " : t.haltCountdown ? L"[held] " : L"", t.offline ? L"(offline) " : L"",
            t.intervalSec, t.msMode ? L"ms" : L"s", (LPCWSTR)t.command);
        Show(w, s, cInfo);
    }
    void CmdTimer(CChatWnd* w, CString tname, CString arg) {
        Net* net = w->net; arg.Trim();
        bool oFlag = false, cFlag = false, mFlag = false, dFlag = false, eFlag = false, iFlag = false, pFlag = false, PFlag = false, rFlagSw = false;
        while (arg.Left(1) == L"-") {
            CString swTok = Word(arg);
            for (int i = 1; i < swTok.GetLength(); i++) {
                wchar_t c = swTok[i];
                if (c == L'o') oFlag = true; else if (c == L'c') cFlag = true; else if (c == L'm' || c == L'h') mFlag = true;
                else if (c == L'd') dFlag = true; else if (c == L'e') eFlag = true; else if (c == L'i') iFlag = true;
                else if (c == L'p') pFlag = true; else if (c == L'P') PFlag = true; else if (c == L'r') rFlagSw = true;
                else if (c == L'z') while (i + 1 < swTok.GetLength() && iswdigit(swTok[i + 1])) i++;   // -zN: no Online Timer feature exists here to reset
            }
            arg.TrimLeft();
        }
        if (eFlag) {   // -e: run the matching timer(s) right now, once, without touching their schedule or repeat count
            bool any = false;
            for (auto& t : m_timers) if (GlobMatch(tname, t.name)) { any = true; CChatWnd* fw = Find(t.net, t.winName); if (!fw) fw = t.net ? Status(t.net) : w; if (fw) RunScript(fw, std::vector<CString>{ t.command }, CString()); }
            if (!any) Show(w, L"* No matching timer: " + tname, cPart);
            return;
        }
        CString first = arg; CString w1 = Word(first); CString w1l = w1; w1l.MakeLower();
        if (w1l == L"off") {
            bool wild = tname.Find(L'?') >= 0 || tname.Find(L'*') >= 0;
            size_t before = m_timers.size();
            m_timers.erase(std::remove_if(m_timers.begin(), m_timers.end(), [&](const TimerInfo& t) { return wild ? GlobMatch(tname, t.name) : t.name.CompareNoCase(tname) == 0; }), m_timers.end());
            Show(w, before == m_timers.size() ? L"* No matching timer: " + tname : L"* Timer(s) turned off: " + tname, before == m_timers.size() ? cPart : cInfo);
            return;
        }
        if (pFlag || PFlag || rFlagSw) {   // pause / hold / resume an existing timer -- no reps/interval/command needed for this
            bool any = false;
            for (auto& t : m_timers) if (t.name.CompareNoCase(tname) == 0) {
                any = true;
                if (rFlagSw) { t.paused = false; t.haltCountdown = false; t.nextFire = GetTickCount64() + (ULONGLONG)(t.intervalSec * (t.msMode ? 1 : 1000)); }
                else if (PFlag) t.haltCountdown = true; else if (pFlag) t.paused = true;
            }
            if (!any) Show(w, L"* No such timer: " + tname, cPart);
            return;
        }
        if (arg.IsEmpty()) {   // "/timer1" alone: show that timer's settings (or the usage line, if it doesn't exist)
            for (auto& t : m_timers) if (t.name.CompareNoCase(tname) == 0) { ShowTimerStatus(w, t); return; }
            Show(w, tname.IsEmpty() ? CString(L"* Usage: /timer[N/name] [-switches] [time] <reps> <interval> <command>") : L"* No such timer: " + tname, cPart);
            return;
        }
        // creating (or replacing) a timer: [time] <repetitions> <interval> <command>
        CString a2 = arg; CString t1 = Word(a2);
        bool isClock = t1.Find(L':') >= 0;
        CString timeStr; if (isClock) { timeStr = t1; t1 = Word(a2); }
        CString repsStr = t1, intervalStr = Word(a2), command = a2;
        double intervalVal; 
        if (!IsAllDigits(repsStr) || intervalStr.IsEmpty() || !ParseNum(intervalStr, intervalVal) || command.IsEmpty()) {
            Show(w, L"* Usage: /timer[N/name] [-switches] [time] <reps> <interval> <command>", cPart); return;
        }
        if (tname.IsEmpty()) { int n = 1; while (true) { CString cand; cand.Format(L"%d", n); bool used = false; for (auto& t : m_timers) if (t.name == cand) { used = true; break; } if (!used) break; n++; } tname.Format(L"%d", n); }
        m_timers.erase(std::remove_if(m_timers.begin(), m_timers.end(), [&](const TimerInfo& t) { return t.name.CompareNoCase(tname) == 0; }), m_timers.end());   // replaces any existing timer of the same name
        TimerInfo t;
        t.name = tname; t.net = net; t.winName = w->m_name;
        t.offline = oFlag || !(net && net->conn);   // matches mIRC's own default: online if currently connected, offline otherwise, unless -o forces it
        t.msMode = mFlag; t.catchUp = cFlag; t.dynAssoc = iFlag;
        t.totalReps = _wtoi(repsStr); t.repsLeft = t.totalReps;
        t.intervalSec = t.msMode ? intervalVal / 1000.0 : intervalVal;
        t.command = TimerPreEval(w, command);
        ULONGLONG startDelayMs = 0;
        if (isClock) {
            int hh = 0, mm = 0, ss = 0; int c1 = timeStr.Find(L':');
            hh = _wtoi(timeStr.Left(c1)); CString rest = timeStr.Mid(c1 + 1); int c2 = rest.Find(L':');
            if (c2 >= 0) { mm = _wtoi(rest.Left(c2)); ss = _wtoi(rest.Mid(c2 + 1)); } else mm = _wtoi(rest);
            CTime now = CTime::GetCurrentTime();
            CTime target(now.GetYear(), now.GetMonth(), now.GetDay(), hh, mm, ss);
            if (target <= now) target += CTimeSpan(1, 0, 0, 0);
            startDelayMs = (ULONGLONG)(target.GetTime() - now.GetTime()) * 1000ULL;
        }
        t.nextFire = GetTickCount64() + startDelayMs + (ULONGLONG)(t.intervalSec * 1000);
        m_timers.push_back(t);
        m_ltimer = tname;
        StartTimerTickIfNeeded();
        Show(w, L"* Timer" + tname + L" activated.", cInfo);
    }
    void CmdTimers(CChatWnd* w, CString arg) {
        arg.Trim(); CString a = arg; a.MakeLower();
        if (a == L"off") { size_t n = m_timers.size(); m_timers.clear(); CString s; s.Format(L"* %d timer(s) turned off.", (int)n); Show(w, s, cInfo); return; }
        if (m_timers.empty()) { Show(w, L"* No active timers.", cInfo); return; }
        for (auto& t : m_timers) ShowTimerStatus(w, t);
    }
    void StartTimerTickIfNeeded() { if (!m_timerTickId && !m_timers.empty()) m_timerTickId = SetTimer(2002, 100, nullptr); }
    void TimerTick() {
        if (m_timers.empty()) { if (m_timerTickId) { KillTimer(m_timerTickId); m_timerTickId = 0; } return; }
        ULONGLONG now = GetTickCount64();
        for (size_t i = 0; i < m_timers.size();) {
            TimerInfo& t = m_timers[i];
            if (!t.offline && t.net && !t.net->conn) { m_timers.erase(m_timers.begin() + i); continue; }   // online timer: its network disconnected
            if (t.paused || t.haltCountdown || now < t.nextFire) { i++; continue; }
            CChatWnd* fw = Find(t.net, t.winName); if (!fw) fw = t.net ? Status(t.net) : (m_w.empty() ? nullptr : m_w.begin()->second);
            if (fw) RunScript(fw, std::vector<CString>{ t.command }, CString());
            if (t.totalReps > 0 && --t.repsLeft <= 0) { m_timers.erase(m_timers.begin() + i); continue; }
            t.nextFire = t.catchUp ? t.nextFire + (ULONGLONG)(t.intervalSec * 1000) : now + (ULONGLONG)(t.intervalSec * 1000);
            i++;
        }
        if (m_timers.empty() && m_timerTickId) { KillTimer(m_timerTickId); m_timerTickId = 0; }
    }

    void CmdDns(CChatWnd* w, CString arg) {
        Net* net = w->net; arg.Trim();
        bool ipv4 = false, ipv6 = false, cFlag = false, hFlag = false, mFlag = false, nFlag = false;
        if (arg.Left(1) == L"-") {
            int i = 1; for (; i < arg.GetLength() && arg[i] != L' '; i++) {
                wchar_t c = arg[i];
                if (c == L'4') ipv4 = true; else if (c == L'6') ipv6 = true; else if (c == L'c') cFlag = true;
                else if (c == L'h') hFlag = true; else if (c == L'm') mFlag = true; else if (c == L'n') nFlag = true;
            }
            arg = arg.Mid(i); arg.TrimLeft();
        }
        if (!ipv4 && !ipv6) { ipv4 = true; ipv6 = true; }   // neither given = both
        if (cFlag) {
            if (m_dnsQueue.size() > 1) m_dnsQueue.erase(m_dnsQueue.begin() + 1, m_dnsQueue.end());   // keeps the one in progress (the front), per mIRC's own -c behavior
            Show(w, L"* DNS queue cleared (the request in progress, if any, keeps going).", cInfo);
            if (arg.IsEmpty()) return;
        }
        if (arg.IsEmpty()) {   // no query given: show the current queue
            if (m_dnsQueue.empty()) { Show(w, L"* No DNS requests queued.", cInfo); return; }
            for (size_t i = 0; i < m_dnsQueue.size(); i++) { DnsRequest& r = m_dnsQueue[i]; CString s; s.Format(L"* [%d] %s - %s", (int)i + 1, (LPCWSTR)r.query, (LPCWSTR)r.status); Show(w, s, cInfo); }
            return;
        }
        std::vector<CString> tok = PlayTokenize(arg);
        CString nsServer;
        if (nFlag) { if (tok.empty()) { Show(w, L"* -n needs a name server address.", cPart); return; } nsServer = tok.front(); tok.erase(tok.begin()); }
        if (tok.empty()) { Show(w, L"* Usage: /dns [-46chmn] [name server] [nick|address]", cPart); return; }
        CString query = tok.front();
        DnsRequest r; r.id = ++m_dnsSeq; r.net = net; r.winName = w->m_name; r.query = query; r.nsServer = nsServer;
        r.ipv4 = ipv4; r.ipv6 = ipv6; r.hostForce = hFlag; r.multi = mFlag;
        IN_ADDR a4; IN6_ADDR a6;
        bool looksIp = InetPtonW(AF_INET, query, &a4) == 1 || InetPtonW(AF_INET6, query, &a6) == 1;
        if (looksIp) r.reverse = true;
        else if (!hFlag && query.Find(L'.') < 0) { r.isNickname = true; r.status = L"waiting for userhost"; }   // no dot, not forced as a hostname: treat as a nick
        m_dnsQueue.push_back(r);
        Show(w, L"* Queued DNS request: " + query, cInfo);
        if (r.isNickname) { m_pendingUserhost[CString(query).MakeLower()] = r.id; Send(net, L"USERHOST " + query); }
        StartNextDnsIfIdle();
    }
    void StartNextDnsIfIdle() {
        if (m_dnsQueue.empty()) return;
        DnsRequest& r = m_dnsQueue.front();
        if (r.status != L"queued") return;   // already resolving, waiting on a userhost reply, done, or errored
        r.status = L"resolving";
        auto* job = new DnsJob();
        job->id = r.id; job->hwnd = m_hWnd;
        job->query = (LPCWSTR)(r.resolvedHost.IsEmpty() ? r.query : r.resolvedHost);
        job->nsServer = (LPCWSTR)r.nsServer;
        job->ipv4 = r.ipv4; job->ipv6 = r.ipv6; job->multi = r.multi; job->reverse = r.reverse;
        uintptr_t th = _beginthreadex(nullptr, 0, DnsWorkerProc, job, 0, nullptr);
        if (th) CloseHandle((HANDLE)th); else { delete job; r.status = L"error"; r.error = L"Couldn't start the resolver thread."; ReportDnsResult(r); m_dnsQueue.erase(m_dnsQueue.begin()); StartNextDnsIfIdle(); }
    }
    void ReportDnsResult(const DnsRequest& r) {
        CChatWnd* w = Find(r.net, r.winName); if (!w) w = Status(r.net);
        if (!r.error.IsEmpty()) { Show(w, L"* DNS: " + r.query + L" - " + r.error, cPart); return; }
        if (r.multi) {
            Show(w, L"* DNS records for " + r.query + L":", cInfo);
            for (auto& rec : r.records) Show(w, L"* " + rec.first + L": " + rec.second, cInfo);
        } else if (r.reverse) {
            for (auto& a : r.addrs) Show(w, L"* " + r.query + L" resolved to " + a, cInfo);
        } else {
            CString list; for (size_t i = 0; i < r.addrs.size(); i++) { if (i) list += L", "; list += r.addrs[i]; }
            Show(w, L"* " + (r.resolvedHost.IsEmpty() ? r.query : r.query + L" (" + r.resolvedHost + L")") + L" resolved to " + list, cInfo);
        }
    }
    afx_msg LRESULT OnDnsResult(WPARAM wp, LPARAM lp) {
        std::unique_ptr<DnsJob> job((DnsJob*)lp);   // always delete it, however this turns out
        int id = (int)wp;
        for (auto& r : m_dnsQueue) if (r.id == id) {
            r.error = job->error.c_str();
            for (auto& a : job->addrs) r.addrs.push_back(a.c_str());
            for (auto& rec : job->records) r.records.push_back({ CString(rec.first.c_str()), CString(rec.second.c_str()) });
            r.status = r.error.IsEmpty() ? L"done" : L"error";
            if (r.multi && r.error.IsEmpty()) m_lastDnsRecords = r.records;
            ReportDnsResult(r);
            break;
        }
        if (!m_dnsQueue.empty() && (m_dnsQueue.front().status == L"done" || m_dnsQueue.front().status == L"error")) m_dnsQueue.erase(m_dnsQueue.begin());
        StartNextDnsIfIdle();
        return 0;
    }

    // ---- Colors dialog: named schemes, stored in IRC.ini as [colors] n0=Name,c1,c2,...,c10 (see CColorsDlg) ----
    static CString PackColor(COLORREF c) { CString s; s.Format(c == CLR_NONE ? L"-1" : L"%d", (int)c); return s; }
    static COLORREF UnpackColor(const CString& s) { long v = _wtol(s); return v < 0 ? CLR_NONE : (COLORREF)v; }
    void SeedColorSchemes() {   // used when [colors] doesn't exist yet: one scheme matching the app's built-in defaults
        ColorScheme d; d.name = L"Default";
        m_schemes.clear(); m_schemes.push_back(d); m_curScheme = 0;
    }
    void SaveColors() {
        CWinApp* a = AfxGetApp();
        WritePrivateProfileStringW(L"colors", nullptr, nullptr, IniPath(L"IRC.ini"));   // drop the section, then rewrite it in order
        for (size_t i = 0; i < m_schemes.size(); i++) {
            const ColorScheme& s = m_schemes[i];
            CString line = s.name;
            const COLORREF vals[22] = { 
                s.normal,
                s.ctcp,
                s.highlight,
                s.invite,
                s.join,
                s.part,
                s.quit,
                s.mode,
                s.topic,
                s.kick,
                s.nickname,
                s.own,
                s.notice,
                s.action,
                s.other,
                s.info,
                s.info2,
                s.wallops,
                s.whois,
                s.chatBg, 
                s.editBg, 
                s.nickBg 
            };
            for (int k = 0; k < 22; k++) line += L"," + PackColor(vals[k]);
            CString key; key.Format(L"n%d", (int)i);
            a->WriteProfileString(L"colors", key, line);
        }
        a->WriteProfileInt(L"colors", L"active", m_curScheme);
    }
    void LoadColors() {
        m_schemes.clear();
        CString path = IniPath(L"IRC.ini");
        std::vector<wchar_t> buf(65536, 0);
        DWORD n = GetPrivateProfileSectionW(L"colors", buf.data(), (DWORD)buf.size(), path);
        for (wchar_t* p = buf.data(); n && *p; p += wcslen(p) + 1) {
            CString line = p; int eq = line.Find(L'='); if (eq <= 0) continue;
            CString key = line.Left(eq), val = line.Mid(eq + 1);
            if (key.Left(1).CompareNoCase(L"n") != 0 || !iswdigit(key[1])) continue;   // skips the separate "active" key
            ColorScheme s; int pos = 0; s.name = val.Tokenize(L",", pos);
            COLORREF* slots[22] = {             
                &s.normal,
                &s.ctcp,
                &s.highlight,
                &s.invite,
                &s.join,
                &s.part,
                &s.quit,
                &s.mode,
                &s.topic,
                &s.kick,
                &s.nickname,
                &s.own,
                &s.notice,
                &s.action,
                &s.other,
                &s.info,
                &s.info2,
                &s.wallops,
                &s.whois,
                &s.chatBg,
                &s.editBg,
                &s.nickBg
            };
            for (int k = 0; k < 22 && pos != -1; k++) *slots[k] = UnpackColor(val.Tokenize(L",", pos));
            if (!s.name.IsEmpty()) m_schemes.push_back(s);
        }
        if (m_schemes.empty()) { SeedColorSchemes(); SaveColors(); return; }
        m_curScheme = AfxGetApp()->GetProfileInt(L"colors", L"active", 0);
        if (m_curScheme < 0 || m_curScheme >= (int)m_schemes.size()) m_curScheme = 0;
    }
    ColorScheme& CurScheme() {   // guards against an empty/out-of-range m_schemes (shouldn't normally happen once Start() has run)
        if (m_schemes.empty()) SeedColorSchemes();
        if (m_curScheme < 0 || m_curScheme >= (int)m_schemes.size()) m_curScheme = 0;
        return m_schemes[m_curScheme];
    }

    void PushSchemeColors(const ColorScheme& s) { 
        cText = s.normal; 
        cCTCP = s.ctcp;
        cHighlight = s.highlight;
        cInvite = s.invite; 
        cJoin = s.join;
        cPart = s.part;
        cQuit = s.quit;
        cMode = s.mode; 
        cTopic = s.topic;  
        cKick = s.kick; 
        cNickname = s.nickname; 
        cOwn = s.own; 
        cNotice = s.notice; 
        cInfo = s.info;
        cInfo2 = s.info2;
        cAction = s.action; 
        cOther = s.other;
        cWallops = s.wallops; 
        cWhois = s.whois;
    }
    void ApplyColorScheme(int idx) {   // pushes the scheme's colors into the global text-color variables and every open window
        if (idx < 0 || idx >= (int)m_schemes.size()) return;
        m_curScheme = idx; const ColorScheme& s = m_schemes[idx];
        PushSchemeColors(s);
        for (auto& kv : m_w) kv.second->ApplyColors(s.chatBg, s.editBg, s.nickBg);
        SaveColors();
    }

    void LoadSkinPaths() {
        CWinApp* a = AfxGetApp();
        m_swSkinPath = a->GetProfileString(L"background", L"switchbar", L"");
        m_tbSkinPath = a->GetProfileString(L"background", L"toolbar", L"");
        m_mdiSkinPath = a->GetProfileString(L"background", L"mdi", L"");
    }
    void SaveSkinPaths() {
        CWinApp* a = AfxGetApp();
        a->WriteProfileString(L"background", L"switchbar", m_swSkinPath);
        a->WriteProfileString(L"background", L"toolbar", m_tbSkinPath);
        a->WriteProfileString(L"background", L"mdi", m_mdiSkinPath);
    }
    void LoadSkinImages() {   // (re)loads the actual pictures from whatever paths are currently set
        m_swSkinBmp.reset(); m_tbSkinBmp.reset(); m_mdiSkinBmp.reset();
        if (!m_swSkinPath.IsEmpty()) {
            auto bmp = std::make_unique<Gdiplus::Bitmap>(ResolveSkinPath(m_swSkinPath));
            if (bmp->GetLastStatus() == Gdiplus::Ok) m_swSkinBmp = std::move(bmp);
        }
        if (!m_tbSkinPath.IsEmpty()) {
            auto bmp = std::make_unique<Gdiplus::Bitmap>(ResolveSkinPath(m_tbSkinPath));
            if (bmp->GetLastStatus() == Gdiplus::Ok) m_tbSkinBmp = std::move(bmp);
        }
        if (!m_mdiSkinPath.IsEmpty()) {
            auto bmp = std::make_unique<Gdiplus::Bitmap>(ResolveSkinPath(m_mdiSkinPath));
            if (bmp->GetLastStatus() == Gdiplus::Ok) m_mdiSkinBmp = std::move(bmp);
        }
        m_sw.skin = m_swSkinBmp.get();
    }
    CString PickSkinFile() {
        CFileDialog fd(TRUE, L"png", nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
            L"Image Files (*.bmp;*.jpg;*.jpeg;*.png;*.gif)|*.bmp;*.jpg;*.jpeg;*.png;*.gif|All Files (*.*)|*.*||", this);
        return fd.DoModal() == IDOK ? fd.GetPathName() : CString();
    }
    // Called after every reload so nothing is left holding a pointer into a Gdiplus::Bitmap that LoadSkinImages()
    // just destroyed and replaced -- LoadSkinImages() reloads both skins together (whichever one's path changed),
    // so every consumer needs refreshing every time, not just the one the caller happens to be changing.
    void RefreshSkinConsumers() {
        m_sw.skin = m_swSkinBmp.get(); m_sw.Invalidate();
        m_mdiWrap.skin = m_mdiSkinBmp.get(); if (m_mdiWrap.m_hWnd) m_mdiWrap.Invalidate();
        if (m_tb.m_hWnd) m_tb.Invalidate();
    }
    void SetSkin(bool toolbar, const CString& absPathOrEmpty) {   // empty = clear that skin back to plain color
        CString stored = absPathOrEmpty.IsEmpty() ? CString() : RelativizeSkinPath(absPathOrEmpty);
        (toolbar ? m_tbSkinPath : m_swSkinPath) = stored;
        SaveSkinPaths(); LoadSkinImages(); RefreshSkinConsumers();
    }
    void SetMdiSkin(const CString& absPathOrEmpty) {   // empty = clear back to the normal workspace colour
        m_mdiSkinPath = absPathOrEmpty.IsEmpty() ? CString() : RelativizeSkinPath(absPathOrEmpty);
        SaveSkinPaths(); LoadSkinImages(); RefreshSkinConsumers();
    }
    void LoadOpts() {
        CWinApp* a = AfxGetApp();
        m_defOpts.host = a->GetProfileString(L"Conn", L"Host", m_defOpts.host); m_defOpts.port = a->GetProfileInt(L"Conn", L"Port", m_defOpts.port);
        m_defOpts.nick = a->GetProfileString(L"Conn", L"Nick", m_defOpts.nick); m_defOpts.user = a->GetProfileString(L"Conn", L"User", m_defOpts.user);
        m_defOpts.real = a->GetProfileString(L"Conn", L"Real", m_defOpts.real); m_defOpts.autojoin = a->GetProfileString(L"Conn", L"Join", m_defOpts.autojoin);
        m_defOpts.tls = a->GetProfileInt(L"Conn", L"TLS", 0); m_defOpts.lax = a->GetProfileInt(L"Conn", L"Lax", 0);
        m_swTop = a->GetProfileInt(L"Conn", L"SwTop", 1) != 0;
    }
    void SaveOpts() {   // password is deliberately not saved; this is just the template that pre-fills the next Connect dialog
        CWinApp* a = AfxGetApp();
        a->WriteProfileString(L"Conn", L"Host", m_defOpts.host); a->WriteProfileInt(L"Conn", L"Port", m_defOpts.port);
        a->WriteProfileString(L"Conn", L"Nick", m_defOpts.nick); a->WriteProfileString(L"Conn", L"User", m_defOpts.user);
        a->WriteProfileString(L"Conn", L"Real", m_defOpts.real); a->WriteProfileString(L"Conn", L"Join", m_defOpts.autojoin);
        a->WriteProfileInt(L"Conn", L"TLS", m_defOpts.tls); a->WriteProfileInt(L"Conn", L"Lax", m_defOpts.lax);
    }
    void LoadBookmarks() {   // servers.ini is separate from MiniIRC.ini — a plain bookmark list, not app settings
        m_bookmarks.clear();
        CString path = IniPath(L"servers.ini");
        int n = GetPrivateProfileIntW(L"Servers", L"Count", 0, path);
        wchar_t buf[256];
        for (int i = 0; i < n; i++) {
            CString sec; sec.Format(L"Server%d", i);
            Bookmark e;
            GetPrivateProfileStringW(sec, L"Name", L"", buf, 256, path); e.name = buf;
            GetPrivateProfileStringW(sec, L"Host", L"", buf, 256, path); e.o.host = buf;
            e.o.port = GetPrivateProfileIntW(sec, L"Port", 6667, path);
            GetPrivateProfileStringW(sec, L"Nick", L"YourNickname", buf, 256, path); e.o.nick = buf;
            GetPrivateProfileStringW(sec, L"User", L"irc", buf, 256, path); e.o.user = buf;
            GetPrivateProfileStringW(sec, L"Real", L"IRC user", buf, 256, path); e.o.real = buf;
            GetPrivateProfileStringW(sec, L"Join", L"", buf, 256, path); e.o.autojoin = buf;
            e.o.tls = GetPrivateProfileIntW(sec, L"TLS", 0, path); e.o.lax = GetPrivateProfileIntW(sec, L"Lax", 0, path);
            if (!e.name.IsEmpty() || !e.o.host.IsEmpty()) m_bookmarks.push_back(e);
        }
    }
    void SaveBookmarks() {   // password is deliberately never saved here either
        CString path = IniPath(L"servers.ini");
        ::DeleteFileW(path);   // simplest way to cleanly rewrite the whole list, since entries can be deleted/reordered
        CString cs; cs.Format(L"%d", (int)m_bookmarks.size());
        WritePrivateProfileStringW(L"Servers", L"Count", cs, path);
        for (size_t i = 0; i < m_bookmarks.size(); i++) {
            CString sec; sec.Format(L"Server%d", (int)i); auto& e = m_bookmarks[i];
            WritePrivateProfileStringW(sec, L"Name", e.name, path); WritePrivateProfileStringW(sec, L"Host", e.o.host, path);
            CString ps; ps.Format(L"%d", e.o.port); WritePrivateProfileStringW(sec, L"Port", ps, path);
            WritePrivateProfileStringW(sec, L"Nick", e.o.nick, path); WritePrivateProfileStringW(sec, L"User", e.o.user, path);
            WritePrivateProfileStringW(sec, L"Real", e.o.real, path); WritePrivateProfileStringW(sec, L"Join", e.o.autojoin, path);
            WritePrivateProfileStringW(sec, L"TLS", e.o.tls ? L"1" : L"0", path); WritePrivateProfileStringW(sec, L"Lax", e.o.lax ? L"1" : L"0", path);
        }
    }
    afx_msg void OnAbout() { CAboutDlg d(this); d.DoModal(); }
    afx_msg void OnServerList() {
        CServerListDlg d(m_bookmarks, this);
        d.DoModal();   // Close returns IDCANCEL either way; connectIdx tells us whether "Connect" was used
        SaveBookmarks();   // persist any add/edit/delete the user made, regardless of how the dialog was closed
        if (d.connectIdx < 0 || d.connectIdx >= (int)m_bookmarks.size()) return;
        Bookmark& e = m_bookmarks[d.connectIdx];
        auto* a = dynamic_cast<CChatWnd*>(MDIGetActive());
        Net* net = (a && a->net && !a->net->conn) ? a->net : NewNet();   // reuse an idle network rather than always adding one
        net->o = e.o; net->nick = e.o.nick; net->tag = e.o.host;
        Status(net);
        Connect(net, e.o.host, e.o.port);
    }

    void LoadFavs() {
        m_favs.clear();
        CString path = IniPath(L"channels.ini");
        int n = GetPrivateProfileIntW(L"Channels", L"Count", 0, path);
        wchar_t buf[256];
        for (int i = 0; i < n; i++) {
            CString sec; sec.Format(L"Chan%d", i);
            ChanFav e;
            GetPrivateProfileStringW(sec, L"Name", L"", buf, 256, path); e.chan = buf;
            GetPrivateProfileStringW(sec, L"Key", L"", buf, 256, path); e.key = buf;
            GetPrivateProfileStringW(sec, L"Net", L"", buf, 256, path); e.net = buf;
            if (!e.chan.IsEmpty()) m_favs.push_back(e);
        }
    }
    void SaveFavs() {
        CString path = IniPath(L"channels.ini");
        ::DeleteFileW(path);
        CString cs; cs.Format(L"%d", (int)m_favs.size());
        WritePrivateProfileStringW(L"Channels", L"Count", cs, path);
        for (size_t i = 0; i < m_favs.size(); i++) {
            CString sec; sec.Format(L"Chan%d", (int)i); auto& e = m_favs[i];
            WritePrivateProfileStringW(sec, L"Name", e.chan, path);
            WritePrivateProfileStringW(sec, L"Key", e.key, path);
            WritePrivateProfileStringW(sec, L"Net", e.net, path);
        }
    }
    afx_msg void OnChanFavs() {
        CFavDlg d(m_favs, this);
        d.DoModal();
        SaveFavs();   // persist any add/delete regardless of how the dialog was closed
        if (d.joinIdx < 0 || d.joinIdx >= (int)m_favs.size()) return;
        ChanFav& e = m_favs[d.joinIdx];
        auto* a = dynamic_cast<CChatWnd*>(MDIGetActive());
        Net* net = a ? a->net : nullptr;
        if (!net || !net->conn) { AfxMessageBox(L"Connect to a server first, then use Channel Favorites to join."); return; }
        Send(net, L"JOIN " + e.chan + (e.key.IsEmpty() ? CString() : L" " + e.key));
    }

    // A small 4-square color-palette icon for the Colors button. The external toolbar.bmp resource (when linked in)
    // only has the original 7 icons, so this one is always drawn programmatically into the 8th image-list slot,
    // whichever path BuildToolbar takes below.
    static void DrawColorsGlyph(CDC& mem, int baseX) {
        int x = baseX + 3, y = 3;
        auto sq = [&](int dx, int dy, COLORREF c) {
            CBrush br(c); CBrush* ob = mem.SelectObject(&br); CPen pn(PS_SOLID, 1, RGB(40, 40, 40)); CPen* op = mem.SelectObject(&pn);
            mem.Rectangle(CRect(x + dx, y + dy, x + dx + 5, y + dy + 5));
            mem.SelectObject(ob); mem.SelectObject(op);
        };
        sq(0, 0, RGB(220,30,30)); sq(5, 0, RGB(30,160,30)); sq(0, 5, RGB(30,90,220)); sq(5, 5, RGB(230,180,0));
    }
    static void DrawOnlineTimerGlyph(CDC& mem, int baseX) {   // a small clock face with two hands, for the Online Timer button
        CPen pn(PS_SOLID, 1, RGB(20, 110, 70)); CPen* op = mem.SelectObject(&pn);
        CBrush* ob = (CBrush*)mem.SelectStockObject(NULL_BRUSH);
        mem.Ellipse(CRect(baseX + 2, 2, baseX + 14, 14));
        int cx = baseX + 8, cy = 8;
        mem.MoveTo(cx, cy); mem.LineTo(cx, cy - 4);       // minute hand
        mem.MoveTo(cx, cy); mem.LineTo(cx + 3, cy + 1);   // hour hand
        mem.SelectObject(op); mem.SelectObject(ob);
    }
    void BuildToolbar() {   // real icons from the optional resource bitmap; falls back to plain drawn glyphs if MiniIRC.rc wasn't linked in
        const int N = 9;
        CBitmap resBmp;
        bool haveRes = resBmp.LoadBitmap(102) != 0;   // id 102 in MiniIRC.rc ("toolbar.bmp"); absent in the plain one-file build
        int W = haveRes ? 24 : 16, H = W; m_tbIcon = W;
        m_tbImg.Create(W, H, ILC_COLOR24 | ILC_MASK, N, 0);
        if (haveRes) {
            m_tbImg.Add(&resBmp, RGB(255, 0, 255));   // strip order: connect, disconnect, server list, cascade, tile, help, favorites, colors, online timer (9 icons)
        } else {
            CClientDC scr(this); CDC mem; mem.CreateCompatibleDC(&scr);
            CBitmap bmp; bmp.CreateCompatibleBitmap(&scr, W * N, H);
            CBitmap* oldBmp = mem.SelectObject(&bmp);
            CBrush maskBg(RGB(255, 0, 255)); mem.FillRect(CRect(0, 0, W * N, H), &maskBg);
            auto glyph = [&](int i, COLORREF c, bool round) {
                CRect r(i * W + 3, 3, i * W + 13, 13);
                CBrush br(c); CBrush* ob = mem.SelectObject(&br); CPen pn(PS_SOLID, 1, RGB(40, 40, 40)); CPen* op = mem.SelectObject(&pn);
                if (round) mem.Ellipse(r); else mem.Rectangle(r);
                mem.SelectObject(ob); mem.SelectObject(op);
            };
            glyph(0, RGB(0, 160, 0), true); glyph(1, RGB(190, 0, 0), true);
            { CBrush br(RGB(120, 80, 170)); CBrush* ob = mem.SelectObject(&br); CPen pn(PS_SOLID, 1, RGB(40, 40, 40)); CPen* op = mem.SelectObject(&pn);
              for (int k = 0; k < 3; k++) mem.Rectangle(CRect(2 * W + 3, 4 + k * 4, 2 * W + 13, 6 + k * 4));   // 3 bars = "list" glyph
              mem.SelectObject(ob); mem.SelectObject(op); }
            glyph(3, RGB(70, 110, 200), false); glyph(4, RGB(70, 110, 200), false);
            { CPen pn(PS_SOLID, 2, RGB(10, 130, 140)); CPen* op = mem.SelectObject(&pn); CBrush* ob = (CBrush*)mem.SelectStockObject(NULL_BRUSH);
              mem.Ellipse(CRect(5 * W + 3, 3, 5 * W + 13, 13));
              mem.SetTextColor(RGB(10, 130, 140)); mem.SetBkMode(TRANSPARENT);
              mem.DrawText(L"?", 1, CRect(5 * W + 3, 2, 5 * W + 13, 13), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
              mem.SelectObject(op); mem.SelectObject(ob); }   // "?" = help glyph
            { CBrush br(RGB(230, 160, 0)); CBrush* ob = mem.SelectObject(&br); CPen pn(PS_SOLID, 1, RGB(140, 90, 0)); CPen* op = mem.SelectObject(&pn);
              int cx = 6 * W + 8, cy = 8;   // 5-point star, outer radius ~6, inner ~2.5, centred in the 7th cell
              static const int off[10][2] = { {0,-6},{1,-2},{6,-2},{2,1},{4,5},{0,3},{-4,5},{-2,1},{-6,-2},{-1,-2} };
              CPoint pts[10]; for (int k = 0; k < 10; k++) pts[k] = CPoint(cx + off[k][0], cy + off[k][1]);
              mem.Polygon(pts, 10);
              mem.SelectObject(ob); mem.SelectObject(op); }   // star = favorites glyph
            DrawOnlineTimerGlyph(mem, 7 * W);   // 8th cell: Online Timer
            DrawColorsGlyph(mem, 8 * W);        // 9th cell: Colors
            mem.SelectObject(oldBmp);
            m_tbImg.Add(&bmp, RGB(255, 0, 255));
        }
        m_tb.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS);
        m_tb.GetToolBarCtrl().SetImageList(&m_tbImg);
        TBBUTTON b[13] = {};
        b[0].iBitmap = 0; b[0].idCommand = IDM_CONNECT; b[0].fsState = TBSTATE_ENABLED; b[0].fsStyle = TBSTYLE_BUTTON;
        b[1].iBitmap = 1; b[1].idCommand = IDM_DISCONNECT; b[1].fsState = TBSTATE_ENABLED; b[1].fsStyle = TBSTYLE_BUTTON;
        b[2].fsStyle = TBSTYLE_SEP;
        b[3].iBitmap = 2; b[3].idCommand = IDM_SERVERS; b[3].fsState = TBSTATE_ENABLED; b[3].fsStyle = TBSTYLE_BUTTON;
        b[4].iBitmap = 6; b[4].idCommand = IDM_CHANFAVS; b[4].fsState = TBSTATE_ENABLED; b[4].fsStyle = TBSTYLE_BUTTON;   // channel favorites
        b[5].iBitmap = 7; b[5].idCommand = IDM_ONLINETIMER; b[5].fsState = TBSTATE_ENABLED; b[5].fsStyle = TBSTYLE_BUTTON;   // online timer
        b[6].iBitmap = 8; b[6].idCommand = IDM_COLORS; b[6].fsState = TBSTATE_ENABLED; b[6].fsStyle = TBSTYLE_BUTTON;    // colors
        b[7].fsStyle = TBSTYLE_SEP;
        b[8].iBitmap = 3; b[8].idCommand = IDM_CASCADE; b[8].fsState = TBSTATE_ENABLED; b[8].fsStyle = TBSTYLE_BUTTON;
        b[9].iBitmap = 4; b[9].idCommand = IDM_TILE; b[9].fsState = TBSTATE_ENABLED; b[9].fsStyle = TBSTYLE_BUTTON;
        b[10].fsStyle = TBSTYLE_SEP;
        b[11].iBitmap = 5; b[11].idCommand = IDM_ABOUT; b[11].fsState = TBSTATE_ENABLED; b[11].fsStyle = TBSTYLE_BUTTON;
        b[12].fsStyle = TBSTYLE_SEP;
        m_tb.GetToolBarCtrl().AddButtons(13, b);
        m_tb.GetToolBarCtrl().SetButtonSize(haveRes ? CSize(36, 34) : CSize(28, 26));
    }
    afx_msg void OnConnectDlg() {   // reuses the active window's network if it's idle/disconnected; otherwise adds a new one (like /server -m)
        auto* a = dynamic_cast<CChatWnd*>(MDIGetActive());
        bool reuse = a && a->net && !a->net->conn;
        Net* net = reuse ? a->net : NewNet();
        CConnDlg d(net->o, this);
        if (d.DoModal() == IDOK) {
            net->nick = net->o.nick; net->tag = net->o.host; m_defOpts = net->o; SaveOpts();
            Status(net);
            Connect(net, net->o.host, net->o.port);
        } else if (!reuse) m_nets.pop_back();   // cancelled: discard the unused network we just created (nothing else references it yet)
    }
    afx_msg void OnFont() {
        LOGFONT lf = m_chatFont;
        CFontDialog dlg(&lf, CF_SCREENFONTS, nullptr, this);
        if (dlg.DoModal() != IDOK) return;
        dlg.GetCurrentFont(&lf); m_chatFont = lf; SaveFont();
        for (auto& kv : m_w) kv.second->ApplyFont(m_chatFont);   // applies to every open window; new text in each uses it too
    }
    afx_msg void OnDisconnect() {   // disconnects whichever network the active window belongs to
        auto* a = dynamic_cast<CChatWnd*>(MDIGetActive());
        if (a && a->net) OnInput(Status(a->net), L"/quit");
    }
    afx_msg void OnCascade() { MDICascade(); }
    afx_msg void OnTile() { MDITile(MDITILE_HORIZONTAL); }
    afx_msg void OnExit() { PostMessage(WM_CLOSE); }
    afx_msg void OnSwTop() { SetSwPos(true); }
    afx_msg void OnSwBottom() { SetSwPos(false); }
    afx_msg void OnUpdateSwTop(CCmdUI* u) { u->SetCheck(m_swTop); }
    afx_msg void OnUpdateSwBottom(CCmdUI* u) { u->SetCheck(!m_swTop); }
    bool m_menuOpen = false;   // true while a TrackPopupMenu is showing; our timer must not touch layout/bars during that
    afx_msg void OnTimer(UINT_PTR id) {
        if (id == 2001) { PlayTick(); return; }   // /play: ticks independently of the UI-refresh timer below, and even while a menu is open
        if (id == 2002) { TimerTick(); return; }   // /timer: same reasoning
        if (id == 2003) { TrayAnimTick(); return; }   // tray icon activity flash: same reasoning
        if (m_menuOpen) return; RefreshBars(); CheckLayout(); TickVars(); UpdateOnlineTimer();
        TipTick(); TipCheckActivation(); SoundTick();
        if (m_identdAutoStopAt && GetTickCount64() >= m_identdAutoStopAt) StopIdentd();
    }
    afx_msg void OnTbRClick(NMHDR*, LRESULT* pResult) {
        *pResult = 0;
        CPoint pt; GetCursorPos(&pt);
        CMenu m; m.CreatePopupMenu();
        m.AppendMenu(MF_STRING, 1, L"Set Background Image...");
        if (!m_tbSkinPath.IsEmpty()) m.AppendMenu(MF_STRING, 2, L"Clear Background Image");
        SetForegroundWindow(); m_menuOpen = true;
        int r = m.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
        m_menuOpen = false; PostMessage(WM_NULL, 0, 0);
        if (r == 1) { CString f = PickSkinFile(); if (!f.IsEmpty()) SetSkin(true, f); }
        else if (r == 2) SetSkin(true, CString());
    }
    afx_msg void OnTbCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) {
        NMTBCUSTOMDRAW* cd = (NMTBCUSTOMDRAW*)pNMHDR;
        *pResult = CDRF_DODEFAULT;
        if (!m_tbSkinBmp) return;   // no skin set: the toolbar draws itself exactly as it always did
        if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) {
            CRect r; m_tb.GetClientRect(r);
            Gdiplus::Graphics g(cd->nmcd.hdc); g.DrawImage(m_tbSkinBmp.get(), 0, 0, r.Width(), r.Height());
            *pResult = CDRF_NOTIFYITEMDRAW;
        } else if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            // The toolbar's own button drawing fills every button with the system button-face color (a near-white
            // on Windows 10/11), which shows up as white boxes around each icon on top of the skin. So with a skin
            // active, draw the buttons ourselves: just the icon (masked), plus a hot/pressed frame.
            TBBUTTONINFO bi = {}; bi.cbSize = sizeof bi; bi.dwMask = TBIF_IMAGE;
            if (m_tb.GetToolBarCtrl().GetButtonInfo((int)cd->nmcd.dwItemSpec, &bi) < 0) return;   // separator: leave it to the default
            CDC* dc = CDC::FromHandle(cd->nmcd.hdc);
            CRect r = cd->nmcd.rc;
            bool down = (cd->nmcd.uItemState & (CDIS_SELECTED | CDIS_CHECKED)) != 0, hot = (cd->nmcd.uItemState & CDIS_HOT) != 0;
            if (down) dc->Draw3dRect(r, ::GetSysColor(COLOR_BTNSHADOW), ::GetSysColor(COLOR_BTNHIGHLIGHT));
            else if (hot) dc->Draw3dRect(r, ::GetSysColor(COLOR_BTNHIGHLIGHT), ::GetSysColor(COLOR_BTNSHADOW));
            CPoint p(r.left + (r.Width() - m_tbIcon) / 2 + (down ? 1 : 0), r.top + (r.Height() - m_tbIcon) / 2 + (down ? 1 : 0));
            m_tbImg.Draw(dc, bi.iImage, p, ILD_TRANSPARENT);
            *pResult = CDRF_SKIPDEFAULT;
        }
    }
    afx_msg void OnInitMenuPopup(CMenu* pMenu, UINT nIndex, BOOL bSysMenu) {
        // CFrameWnd's default handling here auto-disables any item whose command ID has no ON_COMMAND
        // handler in the message map — and it does this for ANY popup shown while we're the owner, not
        // just our own menu bar. Our switchbar context menus use raw ids read via TPM_RETURNCMD, with no
        // ON_COMMAND registered for them on purpose, so the default handling was silently greying every
        // item out (invisible-looking since we never called EnableMenuItem ourselves) right before display.
        // Only let the real menu bar's popups (File / Window / Help) go through the default auto-update,
        // since the Window menu's checkmarks (Switchbar at Top/Bottom) genuinely rely on it.
        if (!bSysMenu) {
            HMENU h = pMenu->GetSafeHmenu();
            bool ours = false;   // scan every top-level item's submenu, since plain string items (Servers/Favorites
            int n = m_menu.GetMenuItemCount();   // shortcuts) shift the real File/Window/Help positions around
            for (int i = 0; i < n && !ours; i++) {
                CMenu* sub = m_menu.GetSubMenu(i);
                if (sub && sub->GetSafeHmenu() == h) ours = true;
            }
            if (!ours) return;   // one of our ad-hoc popups: skip the base class, leave items as we set them
        }
        CMDIFrameWnd::OnInitMenuPopup(pMenu, nIndex, bSysMenu);
    }
    afx_msg void OnSize(UINT nType, int cx, int cy) {
        CMDIFrameWnd::OnSize(nType, cx, cy);   // this positions the menu/status bar and gives the rest to the MDI client
        LayoutBars();
    }
    CRect m_lastSw, m_lastCli;   // the rects we last placed things at, so CheckLayout() can tell if something else moved them
    void LayoutBars() {   // full recompute + carve: only call this for a real resize/explicit change, never blindly on a timer
        RecalcLayout();   // resets the MDI client to its full size first, with no knowledge of our switchbar, so this is the expensive path
        if (!m_sw.m_hWnd || !m_hWndMDIClient) return;
        CRect r; ::GetWindowRect(m_hWndMDIClient, &r); ScreenToClient(&r);
        int h = CSwitchBar::HEIGHT;
        int swY = m_swTop ? r.top : r.bottom - h;
        CRect want(r.left, swY, r.left + r.Width(), swY + h);
        m_sw.SetWindowPos(nullptr, want.left, want.top, want.Width(), want.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
        int cliY = m_swTop ? r.top + h : r.top;
        CRect wantCli(r.left, cliY, r.left + r.Width(), cliY + (std::max)(0L, (long)r.Height() - h));
        ::SetWindowPos(m_hWndMDIClient, nullptr, wantCli.left, wantCli.top, wantCli.Width(), wantCli.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
        m_lastSw = want; m_lastCli = wantCli;   // remember what we just set, so the cheap check below has a baseline
    }
    void CheckLayout() {   // cheap: safe to call every timer tick. Only calls the expensive LayoutBars() if something
        if (!m_sw.m_hWnd || !m_hWndMDIClient) return;              // actually disturbed the switchbar/MDI client since we last set them
        CRect sw; m_sw.GetWindowRect(&sw); ScreenToClient(&sw);
        CRect cli; ::GetWindowRect(m_hWndMDIClient, &cli); ScreenToClient(&cli);
        if (sw != m_lastSw || cli != m_lastCli) LayoutBars();
    }
    void SetSwPos(bool top) { 
        m_swTop = top; 
        AfxGetApp()->WriteProfileInt(L"Conn", L"SwTop", top); 
        LayoutBars(); 
    }
    DECLARE_MESSAGE_MAP()
public:
    void Start() {
        LoadOpts(); 
        LoadFont(); 
        LoadBookmarks(); 
        LoadFavs();
		LoadAliases();
		LoadVars();
		LoadPopups();
		LoadColors(); PushSchemeColors(CurScheme());
		LoadLogging();
		LoadTimestamp();
		LoadOnlineTimer();
		LoadIdentd();
		if (m_identdEnabled && !m_identdOnlyConnecting) StartIdentd();
		LoadTraySettings();
		LoadTipsSettings();
		{
			bool shiftQuitMin = AfxGetApp()->GetProfileInt(L"Tray", L"startMinimizedNext", 0) != 0;
			AfxGetApp()->WriteProfileInt(L"Tray", L"startMinimizedNext", 0);   // one-shot: only applies to the very next startup
			if (m_trayAlwaysShow) ShowTrayIcon();
			if (m_trayMinOnStartup || shiftQuitMin) { ShowWindow(SW_MINIMIZE); ShowWindow(SW_HIDE); if (!m_trayAlwaysShow) ShowTrayIcon(); }
		}
		LoadSkinPaths();
		LoadSkinImages();
		if (m_hWndMDIClient && m_mdiWrap.SubclassWindow(m_hWndMDIClient)) {
			m_mdiWrap.skin = m_mdiSkinBmp.get();
			m_mdiWrap.onBarMenu = [this](CPoint pt) {   // right-click the empty grey workspace behind the MDI windows
				CMenu m; m.CreatePopupMenu();
				m.AppendMenu(MF_STRING, 1, L"Background Image...");
				if (!m_mdiSkinPath.IsEmpty()) m.AppendMenu(MF_STRING, 2, L"Clear Background Image");
				SetForegroundWindow(); m_menuOpen = true;
				int r = m.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
				m_menuOpen = false; PostMessage(WM_NULL, 0, 0);
				if (r == 1) { CString f = PickSkinFile(); if (!f.IsEmpty()) SetMdiSkin(f); }
				else if (r == 2) SetMdiSkin(CString());
			};
		}
        CMenu f, s, c, w, h;
        f.CreatePopupMenu(); 
        f.AppendMenu(MF_STRING, IDM_CONNECT, L"&Connect..."); 
        f.AppendMenu(MF_STRING, IDM_DISCONNECT, L"&Disconnect");
        f.AppendMenu(MF_SEPARATOR); 
        f.AppendMenu(MF_STRING, IDM_FONT, L"&Font...");
        f.AppendMenu(MF_STRING, IDM_ALIASES, L"&Aliases...");
        f.AppendMenu(MF_STRING, IDM_COLORS, L"&Colors...");
        f.AppendMenu(MF_STRING, IDM_LOGGING, L"Lo&gging...");
        f.AppendMenu(MF_STRING, IDM_ONLINETIMER, L"&Online Timer...");
        f.AppendMenu(MF_STRING, IDM_IDENTD, L"&Identd Server...");
        f.AppendMenu(MF_STRING, IDM_TRAY, L"&Tray...");
        f.AppendMenu(MF_STRING, IDM_TIPS, L"T&ips...");
        { CMenu ps; ps.CreatePopupMenu();   // File > Popups: edit each of the five popup menus
          ps.AppendMenu(MF_STRING, IDM_POPEDIT0, L"&Status window...");
          ps.AppendMenu(MF_STRING, IDM_POPEDIT1, L"&Channel window...");
          ps.AppendMenu(MF_STRING, IDM_POPEDIT2, L"&Query window...");
          ps.AppendMenu(MF_STRING, IDM_POPEDIT3, L"&Nick list...");
          ps.AppendMenu(MF_STRING, IDM_POPEDIT4, L"&Menu bar...");
          f.AppendMenu(MF_POPUP, (UINT_PTR)ps.Detach(), L"&Popups"); }
        f.AppendMenu(MF_STRING, IDM_SERVERS, L"&Server List...");
        f.AppendMenu(MF_STRING, IDM_CHANFAVS, L"Channel F&avorites...");
        f.AppendMenu(MF_SEPARATOR); 
        f.AppendMenu(MF_STRING, IDM_EXIT, L"E&xit");
        s.CreatePopupMenu();
        c.CreatePopupMenu();
        w.CreatePopupMenu(); w.AppendMenu(MF_STRING, IDM_CASCADE, L"&Cascade"); 
        w.AppendMenu(MF_STRING, IDM_TILE, L"&Tile");
        w.AppendMenu(MF_SEPARATOR); w.AppendMenu(MF_STRING, IDM_SWTOP, L"Switchbar at &Top"); 
        w.AppendMenu(MF_STRING, IDM_SWBOTTOM, L"Switchbar at &Bottom");
        h.CreatePopupMenu(); 
        h.AppendMenu(MF_STRING, IDM_ABOUT, L"&About IRC...");
        m_menu.CreateMenu();
        m_menu.AppendMenu(MF_POPUP, (UINT_PTR)f.Detach(), L"&File");
        m_menu.AppendMenu(MF_STRING, IDM_SERVERS, L"&Servers");
        m_menu.AppendMenu(MF_STRING, IDM_CHANFAVS, L"&Favorites");
        m_menu.AppendMenu(MF_POPUP, (UINT_PTR)w.Detach(), L"&Window");
		m_menu.AppendMenu(MF_POPUP, (UINT_PTR)h.Detach(), L"&Help");   
        SetMenu(&m_menu); DrawMenuBar();
        RebuildMenuBarPopups();   // the [bpopup] menus from popups.ini go in before "Window"
        static UINT ind[4] = { 0, 0, 0, 0 };
        m_bar.Create(this); m_bar.SetIndicators(ind, 4);
        m_bar.SetPaneInfo(0, 0, SBPS_STRETCH, 100); m_bar.SetPaneInfo(1, 0, SBPS_NORMAL, 130);
        m_bar.SetPaneInfo(2, 0, SBPS_NORMAL, 170); m_bar.SetPaneInfo(3, 0, SBPS_NORMAL, 280);
        BuildToolbar();
        m_sw.Create(this);
        m_sw.onBarMenu = [this](CPoint pt) {
            CMenu m; m.CreatePopupMenu();
            m.AppendMenu(MF_STRING | (m_swTop ? MF_CHECKED : 0), 1, L"Switchbar at Top");
            m.AppendMenu(MF_STRING | (!m_swTop ? MF_CHECKED : 0), 2, L"Switchbar at Bottom");
            m.AppendMenu(MF_SEPARATOR); m.AppendMenu(MF_STRING, 3, L"Set Background Image...");
            if (!m_swSkinPath.IsEmpty()) m.AppendMenu(MF_STRING, 4, L"Clear Background Image");
            SetForegroundWindow();   // required by Windows for the popup to reliably receive clicks at all
            m_menuOpen = true;
            int r = m.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
            m_menuOpen = false;
            PostMessage(WM_NULL, 0, 0);   // MSDN-documented pairing for the above; without it the window can be left in a bad activation state
            if (r == 1) SetSwPos(true);
            else if (r == 2) SetSwPos(false);
            else if (r == 3) { CString f = PickSkinFile(); if (!f.IsEmpty()) SetSkin(false, f); }
            else if (r == 4) SetSkin(false, CString());
        };
        m_bar.onChan = [this](CString c) {   // clicking a channel name in the status bar's "Channels:" pane
            auto* a = dynamic_cast<CChatWnd*>(MDIGetActive());
            if (a && a->net) Goto(a->net, c);
        };
        m_sw.onSel = [this](int i) {   // click a switchbar button -> activate that window
            if (i >= 0 && i < (int)m_tabWnds.size()) Activate(m_tabWnds[i]);
        };
        m_sw.onMenu = [this](int i, CPoint pt) {   // right-click a switchbar button
            if (i < 0 || i >= (int)m_tabWnds.size()) return;
            CMDIChildWnd* mw = m_tabWnds[i];
            if (auto* lw = dynamic_cast<CListWnd*>(mw)) {   // a /list window: just offer Close
                CMenu lm; lm.CreatePopupMenu();
                lm.AppendMenu(MF_STRING, 1, L"Close");
                SetForegroundWindow(); m_menuOpen = true;
                int lcmd = lm.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
                m_menuOpen = false; PostMessage(WM_NULL, 0, 0);
                Activate(lw);
                if (lcmd == 1) lw->PostMessage(WM_CLOSE);
                return;
            }
            CChatWnd* w = (CChatWnd*)mw;
            bool st = w->m_name == L"*status*";
            CMenu m; m.CreatePopupMenu();
            if (st) { 
            m.AppendMenu(MF_STRING, 1, L"Connect..."); 
            m.AppendMenu(MF_STRING, 2, L"Disconnect"); 
            }
            else {
                m.AppendMenu(MF_STRING, 3, w->m_chan ? L"Part / Close" : L"Close");
                if (w->m_chan) m.AppendMenu(MF_STRING, 5, L"Add to Favorites");
            }
            m.AppendMenu(MF_STRING, 4, L"Clear");
            SetForegroundWindow();   // required by Windows for the popup to reliably receive clicks at all
            m_menuOpen = true;
            int cmd = m.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
            m_menuOpen = false;
            PostMessage(WM_NULL, 0, 0);   // MSDN-documented pairing for the above; without it the window can be left in a bad activation state
            Activate(w);   // moved to after the menu closes: doing this beforehand shifted keyboard focus right as
                            // TrackPopupMenu started tracking, which could disrupt its mouse capture mid-click
            switch (cmd) {
                case 1: PostMessage(WM_COMMAND, IDM_CONNECT); break;   // always starts a brand-new network
                case 2: OnInput(w, L"/quit"); break;
                case 3: w->PostMessage(WM_CLOSE); break;
                case 4: w->Clear(); break;
                case 5: {
                    bool dup = false;
                    for (auto& e : m_favs) if (e.chan.CompareNoCase(w->m_name) == 0) { dup = true; break; }
                    if (!dup) { ChanFav e; e.chan = w->m_name; e.net = w->net ? w->net->tag : CString(); m_favs.push_back(e); SaveFavs(); }
                    AfxMessageBox(dup ? L"Already in Channel Favorites." : L"Added to Channel Favorites.");
                    break;
                }
            }
        };
        m_sw.onClose = [this](int i) {   // middle-click a button -> close that window
            if (i < 0 || i >= (int)m_tabWnds.size()) return;
            CMDIChildWnd* mw = m_tabWnds[i];
            if (auto* c = dynamic_cast<CChatWnd*>(mw)) { if (c->m_name == L"*status*") return; }   // never middle-click-close Status
            mw->PostMessage(WM_CLOSE);
        };
        if (!m_sw.m_hWnd) AfxMessageBox(L"Switchbar creation failed");
        RecalcLayout(); LayoutBars(); SetTimer(1, 500, nullptr);
        PostMessage(WM_COMMAND, IDM_CONNECT);
        Net* net = NewNet();   // an idle, disconnected network with just a Status window — lets local commands
        Status(net);           // (/clear, testing the UI, etc.) be tried without ever connecting anywhere
        Note(net, L"IRC ready. Not connected use File > Connect, the toolbar, or /server [-m] host [+port] to connect. "
             L"Ctrl+K/B/U/O/I insert color/bold/underline/reset/italic codes.");
    }
};

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
    ON_WM_CLOSE()
    ON_WM_SYSCOMMAND()
    ON_MESSAGE(WM_APP + 50, OnDnsResult)
    ON_MESSAGE(WM_APP + 51, OnIdentdRequest)
    ON_MESSAGE(WM_APP + 52, OnTrayNotify)
    ON_COMMAND(IDM_CONNECT, OnConnectDlg) 
    ON_COMMAND(IDM_DISCONNECT, OnDisconnect)
    ON_COMMAND(IDM_CASCADE, OnCascade) 
    ON_COMMAND(IDM_TILE, OnTile) 
    ON_COMMAND(IDM_EXIT, OnExit) 
    ON_COMMAND(IDM_FONT, OnFont) ON_COMMAND(IDM_ALIASES, OnAliasEditor) ON_COMMAND(IDM_COLORS, OnColorsDialog) ON_COMMAND(IDM_LOGGING, OnLoggingDialog) ON_COMMAND(IDM_ONLINETIMER, OnOnlineTimerDialog) ON_COMMAND(IDM_IDENTD, OnIdentdDialog) ON_COMMAND(IDM_TRAY, OnTrayDialog) ON_COMMAND(IDM_TIPS, OnTipsDialog) ON_COMMAND_RANGE(IDM_POPEDIT0, IDM_POPEDIT4, OnPopupEditor) ON_COMMAND_RANGE(IDP_BAR, IDP_BAR + 999, OnMenubarPopup) 
    ON_COMMAND(IDM_SERVERS, OnServerList) 
    ON_COMMAND(IDM_CHANFAVS, OnChanFavs) 
	ON_COMMAND(IDM_ABOUT, OnAbout)
    ON_WM_TIMER() 
    ON_WM_SIZE()
	ON_NOTIFY(NM_RCLICK, AFX_IDW_TOOLBAR, OnTbRClick)
	ON_NOTIFY(NM_CUSTOMDRAW, AFX_IDW_TOOLBAR, OnTbCustomDraw)
    ON_COMMAND(IDM_SWTOP, OnSwTop) 
    ON_COMMAND(IDM_SWBOTTOM, OnSwBottom)
    ON_UPDATE_COMMAND_UI(IDM_SWTOP, OnUpdateSwTop) ON_UPDATE_COMMAND_UI(IDM_SWBOTTOM, OnUpdateSwBottom) ON_WM_INITMENUPOPUP()
END_MESSAGE_MAP()

class CIRCClientApp : public CWinApp {
    ULONG_PTR m_gdiplusToken = 0;
public:
    BOOL InitInstance() override {
        CWinApp::InitInstance();
        
        AfxSocketInit(); 
        AfxInitRichEdit2();
		Gdiplus::GdiplusStartupInput gdiInput; Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiInput, nullptr);
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        CString ini = exe; ini = ini.Left(ini.ReverseFind(L'\\') + 1) + L"IRC.ini";
        free((void*)m_pszProfileName); m_pszProfileName = _wcsdup(ini);
        auto* f = new CMainFrame; m_pMainWnd = f;
        f->Create(nullptr, L"IRC Client", WS_OVERLAPPEDWINDOW, CRect(100, 100, 1100, 700));
        HICON hi = (HICON)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(101), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
        if (hi) { f->SetIcon(hi, TRUE); f->SetIcon(hi, FALSE); }   // no-op if IRC.rc wasn't linked in
        f->ShowWindow(SW_SHOW); f->UpdateWindow();
        f->Start();
        return TRUE;
    }
    int ExitInstance() override {
        if (m_gdiplusToken) Gdiplus::GdiplusShutdown(m_gdiplusToken);
        return CWinApp::ExitInstance();
    }
} theApp;
