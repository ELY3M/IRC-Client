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
#include <algorithm>
#define SECURITY_WIN32
#include <sspi.h>
#include <schannel.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")           // prevents a console window regardless of the /link command used
#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")   // Unicode MFC entry point (VS sets this automatically)

#define VERSION L"IRC Client 1.0 - https://github.com/ELY3M/IRC-Client"
#define DEFAULT_FONT L"Fixedsys"

static const COLORREF cText = RGB(0,0,0), cJoin = RGB(0,140,0), cPart = RGB(150,0,0),
                      cNote = RGB(200,110,0), cAct = RGB(150,0,150), cInfo = RGB(0,0,180);

// Remove mIRC control codes (bold, color, reverse, underline, reset)
static CString Strip(const CString& s) {
    CString o; int n = s.GetLength();
    auto dig = [&](int j) { return j < n && iswdigit(s[j]); };
    for (int i = 0; i < n; i++) {
        wchar_t c = s[i];
        if (c == 3) {
            int k = 0; while (k < 2 && dig(i + 1)) { i++; k++; }
            if (k && i + 1 < n && s[i + 1] == L',' && dig(i + 2)) { i += 2; if (dig(i + 1)) i++; }
        } else if (c == 2 || c == 15 || c == 22 || c == 29 || c == 31) {}
        else o += c;
    }
    return o;
}
static CString Word(CString& s) {
    s.TrimLeft(); int i = s.Find(L' '); CString w;
    if (i < 0) { w = s; s.Empty(); } else { w = s.Left(i); s = s.Mid(i + 1); }
    return w;
}
static CString Bare(CString s) { s.TrimLeft(L"@+%&~"); return s; }
static CString IniPath(LPCWSTR name) {   // e.g. IniPath(L"servers.ini") -> full path next to the .exe
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    CString p = exe; return p.Left(p.ReverseFind(L'\\') + 1) + name;
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
enum { IDM_CONNECT = 9001, IDM_DISCONNECT, IDM_CASCADE, IDM_TILE, IDM_EXIT, IDM_SWTOP, IDM_SWBOTTOM, IDM_FONT, IDM_SERVERS, IDM_CHANFAVS, IDM_ABOUT,
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
class CAboutDlg : public CDialog {
    std::vector<WORD> t; int cnt = 0;
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
        t.push_back(210); 
        t.push_back(210);
        t.push_back(0); 
        t.push_back(0); 
        S(L"About IRC"); 
        t.push_back(9); 
        S(DEFAULT_FONT);
        ItemRes(SS_ICON, 10, 10, 24, 24, 500, 101);          // the app icon (MiniIRC.ico) — blank if the optional .rc wasn't linked
        ItemRes(SS_BITMAP, 10, 40, 180, 180, 501, 103);       // the banner image (about.bmp) — likewise blank if not linked
        Item(SS_LEFT, 10, 150, 190, 20, 0xFFFF, 0x0082, L"IRC a mIRC-style IRC client for Windows, built with MFC.");
        Item(BS_DEFPUSHBUTTON | WS_TABSTOP, 80, 180, 48, 16, IDOK, 0x0080, L"OK");
        t[4] = (WORD)cnt;
        InitModalIndirect((LPCDLGTEMPLATE)t.data(), parent);
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
protected:
    afx_msg void OnLButtonUp(UINT, CPoint p) {
        Default();
        long s = 0, e = 0; GetSel(s, e);
        if (s != e || !onLink) return;                         // dragging a selection is not a click
        int idx = CharFromPos(p), li = LineFromChar(idx), st = LineIndex(li);
        CString ln; GetTextRange(st, st + LineLength(idx), ln);
        int q = idx - st, n = ln.GetLength(); if (q > n) q = n;
        int a = q, b = q;
        while (a > 0 && !iswspace(ln[a - 1])) a--;
        while (b < n && !iswspace(ln[b])) b++;
        CPoint pa = PosFromChar(st + a), pb = PosFromChar(st + b);
        if (p.x < pa.x || p.x > pb.x) return;                  // clicked blank space, not the word
        CString w = ln.Mid(a, b - a); w.Trim(L",.;:!?()<>[]'\"");
        if (w.GetLength() > 1 && (w[0] == L'#' || w[0] == L'&')) onLink(w);
    }
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CLogEdit, CRichEditCtrl)
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

// ---------------- MDI child: status / channel / query window ----------------
struct Net;   // forward decl: each chat window belongs to one network (see the Net struct, defined near CMainFrame)

// ---------------- Nick list: right-click a nick for Whois / Query / Notice ----------------
class CNickList : public CListBox {
public:
    std::function<void(CString, CPoint)> onRClick;
protected:
    afx_msg void OnRButtonDown(UINT, CPoint p) {
        BOOL outside = TRUE; int idx = ItemFromPoint(p, outside);
        if (idx < 0 || outside) return;
        SetCurSel(idx);
        CString s; GetText(idx, s);
        CPoint sp = p; ClientToScreen(&sp);
        if (onRClick) onRClick(s, sp);
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
    std::function<void(CChatWnd*, CString, CPoint)> onNickMenu;   // right-click a nick in the user list
    CChatWnd(CString n, bool c) : m_name(n), m_chan(c) {}

    void Put(const CString& t, COLORREF fg, COLORREF bg, DWORD fx) {   // every new run also carries the current font explicitly
        m_out.SetSel(-1, -1);
        CHARFORMAT2 cf = {}; cf.cbSize = sizeof cf;
        cf.dwMask = CFM_COLOR | CFM_BACKCOLOR | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_FACE | CFM_SIZE;
        cf.crTextColor = fg; cf.crBackColor = bg == CLR_NONE ? RGB(255, 255, 255) : bg;
        cf.dwEffects = fx | (m_baseBold ? CFE_BOLD : 0) | (m_baseItalic ? CFE_ITALIC : 0);
        cf.yHeight = m_fontTwips; wcsncpy_s(cf.szFaceName, m_face, LF_FACESIZE - 1);
        m_out.SetSelectionCharFormat(cf);
        m_out.ReplaceSel(t);
    }
    // Renders mIRC codes: ^B bold, ^C fg[,bg], ^I italic, ^O reset, ^R reverse, ^_ underline
    void AddLine(CString s, COLORREF base) {
        static const COLORREF pal[16] = { RGB(255,255,255), RGB(0,0,0), RGB(0,0,127), RGB(0,147,0), RGB(255,0,0), RGB(127,0,0),
            RGB(156,0,156), RGB(252,127,0), RGB(255,255,0), RGB(0,252,0), RGB(0,147,147), RGB(0,255,255), RGB(0,0,252),
            RGB(255,0,255), RGB(127,127,127), RGB(210,210,210) };
        CString ts = CTime::GetCurrentTime().Format(L"[%H:%M] "), plain = ts; long ls = 0, le = 0;
        m_out.SetSel(-1, -1); m_out.GetSel(ls, le);            // remember where this line starts
        Put(ts, base, CLR_NONE, 0);
        COLORREF fg = base, bg = CLR_NONE; DWORD fx = 0; CString seg; int n = s.GetLength();
        auto flush = [&] { if (!seg.IsEmpty()) { plain += seg; Put(seg, fg, bg, fx); seg.Empty(); } };
        auto num = [&](int& i) { int v = -1; for (int k = 0; k < 2 && i + 1 < n && iswdigit(s[i + 1]); k++) v = (v < 0 ? 0 : v * 10) + s[++i] - L'0'; return v; };
        for (int i = 0; i < n; i++) {
            wchar_t c = s[i];
            if (c == 2) { flush(); fx ^= CFE_BOLD; }
            else if (c == 29) { flush(); fx ^= CFE_ITALIC; }
            else if (c == 31) { flush(); fx ^= CFE_UNDERLINE; }
            else if (c == 22) { flush(); COLORREF t = fg; fg = bg == CLR_NONE ? RGB(255,255,255) : bg; bg = t; }
            else if (c == 15) { flush(); fg = base; bg = CLR_NONE; fx = 0; }
            else if (c == 3) {
                flush(); int f = num(i);
                if (f < 0) { fg = base; bg = CLR_NONE; continue; }
                fg = pal[f % 16];
                if (i + 2 < n && s[i + 1] == L',' && iswdigit(s[i + 2])) { i++; bg = pal[num(i) % 16]; }
            }
            else seg += c;
        }
        flush(); Put(L"\r\n", base, CLR_NONE, 0);
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
    void SetTopic(const CString& t) { if (m_chan) m_topic.SetWindowText(Strip(t)); }
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

protected:
    long m_fontTwips = 200; wchar_t m_face[LF_FACESIZE] = L"Consolas"; bool m_baseBold = false, m_baseItalic = false;
    std::vector<CString> m_hist; int m_histPos = -1;   // per-window input history; -1 = not currently browsing it
    CLogEdit m_out; CEdit m_in, m_topic; CNickList m_nicks; CFont m_font;

    afx_msg int OnCreate(LPCREATESTRUCT cs) {
        if (CMDIChildWnd::OnCreate(cs) == -1) return -1;
        CRect z(0, 0, 0, 0);
        m_out.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, z, this, 1);
        m_out.LimitText(0x7FFFFFF); m_out.onLink = [this](CString w) { if (onOpen) onOpen(this, w); };
        m_in.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, z, this, 2);
        m_font.CreatePointFont(100, DEFAULT_FONT);
        m_out.SetFont(&m_font); m_in.SetFont(&m_font);
        if (m_chan) {
            m_topic.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, z, this, 3);
            m_nicks.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_SORT | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, z, this, 4);
            m_nicks.onRClick = [this](CString n, CPoint pt) { if (onNickMenu) onNickMenu(this, Bare(n), pt); };
            m_topic.SetFont(&m_font); m_nicks.SetFont(&m_font);
        }
        return 0;
    }
    afx_msg void OnSize(UINT t, int cx, int cy) {
        CMDIChildWnd::OnSize(t, cx, cy);
        if (!m_in.m_hWnd) return;
        int h = 22, top = m_chan ? h : 0, nw = m_chan ? 140 : 0;
        if (m_chan) m_topic.MoveWindow(0, 0, cx, h);
        m_out.MoveWindow(0, top, cx - nw, cy - top - h);
        if (m_chan) m_nicks.MoveWindow(cx - nw, top, nw, cy - top - h);
        m_in.MoveWindow(0, cy - h, cx, h);
    }
    afx_msg void OnNickDbl() {   // double-click a nick in the list -> open a query window
        int i = m_nicks.GetCurSel(); CString n;
        if (i >= 0 && onOpen) { m_nicks.GetText(i, n); onOpen(this, Bare(n)); }
    }
    afx_msg void OnSetFocus(CWnd*) { m_in.SetFocus(); }
    afx_msg void OnDestroy() { CMDIChildWnd::OnDestroy(); if (onClose) onClose(this); }
    BOOL PreTranslateMessage(MSG* p) override {
        if (p->hwnd == m_in.m_hWnd && p->message == WM_KEYDOWN && GetKeyState(VK_CONTROL) < 0) {   // Ctrl+B/K/U/O/I insert mIRC codes
            wchar_t c = p->wParam == 'B' ? 2 : p->wParam == 'K' ? 3 : p->wParam == 'U' ? 31 : p->wParam == 'O' ? 15 : p->wParam == 'I' ? 29 : 0;
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
        dc.SelectObject(CFont::FromHandle((HFONT)::GetStockObject(DEFAULT_GUI_FONT))); dc.SetBkMode(TRANSPARENT);
        for (int i = 0; i < (int)btns.size(); i++) {
            const Btn& b = btns[i]; CRect r = BtnRect(i, c.Height());
            dc.FillSolidRect(r, ::GetSysColor(b.sel ? COLOR_WINDOW : COLOR_BTNFACE));
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
    DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CSwitchBar, CWnd)
    ON_WM_PAINT() 
    ON_WM_LBUTTONDOWN() 
    ON_WM_RBUTTONUP() 
    ON_WM_MBUTTONUP()
END_MESSAGE_MAP()

// ---------------- Net: one IRC connection (its own socket, nick, options and status text) ----------------
struct Net {
    CIrcSock sock;
    bool conn = false;
    CString nick = L"User";
    Opts o;
    CString state = L"Not connected";
    CString tag;      // short label prefixed onto this network's windows in the switchbar, once there's more than one
    int id = 0;
};

// ---------------- Main frame: connection, protocol, commands ----------------
class CMainFrame : public CMDIFrameWnd {
    std::vector<std::unique_ptr<Net>> m_nets; int m_netSeq = 0; Opts m_defOpts;   // m_defOpts: last-used settings, pre-fills each new Connect dialog
    std::vector<Bookmark> m_bookmarks;   // saved server list (servers.ini)
    std::vector<ChanFav> m_favs;         // saved channel favorites (channels.ini)
    CMenu m_menu; CChanBar m_bar; CSwitchBar m_sw; CToolBar m_tb; CImageList m_tbImg; bool m_swTop = true; LOGFONT m_chatFont = {};
    CString m_bt[4]; int m_seqn = 0; std::vector<CChatWnd*> m_tabWnds;
    std::map<CString, CChatWnd*> m_w;

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
    void Show(CChatWnd* w, const CString& t, COLORREF c = cText) {
        if (!w) return;
        w->AddLine(t, c);
        if (w != static_cast<CChatWnd*>(MDIGetActive())) w->m_act = (std::max)(w->m_act, (c == cText || c == cAct) ? 2 : 1);
    }
    void Activate(CChatWnd* w) { if (w->IsIconic()) MDIRestore(w); MDIActivate(w); }
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
        std::vector<CChatWnd*> ws;
        for (auto& kv : m_w) ws.push_back(kv.second);
        std::sort(ws.begin(), ws.end(), [](CChatWnd* x, CChatWnd* y) {   // group by network first, then creation order within it
            int nx = x->net ? x->net->id : 0, ny = y->net ? y->net->id : 0;
            return nx != ny ? nx < ny : x->m_seq < y->m_seq;
        });
        auto* a = static_cast<CChatWnd*>(MDIGetActive());
        if (a) a->m_act = 0;                                   // activity clears once the window is active
        bool multi = m_nets.size() > 1;                        // more than one network: prefix window labels with its tag
        std::vector<CSwitchBar::Btn> bs; CString chans; m_tabWnds = ws;
        for (auto* w : ws) {
            CSwitchBar::Btn b; CString lbl = w->m_name == L"*status*" ? CString(L"Status") : w->m_name;
            b.text = (multi && w->net) ? (w->net->tag + L": " + lbl) : lbl;
            b.act = w->m_act; b.sel = (w == a); bs.push_back(b);
            if (w->m_chan && (!a || w->net == a->net)) chans += w->m_name + L" ";   // only the active window's network
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

    CChatWnd* Open(Net* net, const CString& name, bool chan) {
        if (auto* e = Find(net, name)) return e;
        auto* w = new CChatWnd(name, chan);
        w->net = net;
        w->onInput = [this](CChatWnd* c, CString s) { OnInput(c, s); };
        w->onClose = [this](CChatWnd* c) { Forget(c); };
        w->onOpen = [this](CChatWnd* c, CString t) { Goto(c->net, t); };
        w->onNickMenu = [this](CChatWnd* c, CString nick, CPoint pt) { ShowNickMenu(c, nick, pt); };
        w->m_seq = ++m_seqn;
        w->Create(nullptr, name, WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, rectDefault, this);
        w->ApplyFont(m_chatFont);
        m_w[Key(net, name)] = w;
        return w;
    }
    void Forget(CChatWnd* c) {   // user closed the window
        Net* net = c->net;
        for (auto i = m_w.begin(); i != m_w.end(); ++i)
            if (i->second == c) { if (c->m_chan && net && net->conn) Send(net, L"PART " + c->m_name); m_w.erase(i); break; }
        if (net && c->m_name == L"*status*" && net->conn) { Send(net, CString(VERSION)); net->sock.Close(); net->conn = false; }
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
        if (action) { Send(net, L"PRIVMSG " + target + L" :" + CString(wchar_t(1)) + L"ACTION " + text + CString(wchar_t(1))); Show(w, L"* " + net->nick + L" " + text, cAct); }
        else        { Send(net, L"PRIVMSG " + target + L" :" + text); Show(w, L"<" + net->nick + L"> " + text); }
    }
    void Connect(Net* net, const CString& host, UINT port) {
        if (net->sock.m_hSocket != INVALID_SOCKET) net->sock.Close();
        net->conn = false; net->sock.buf.Empty(); net->sock.sendq.clear();
        delete net->sock.tls; net->sock.tls = nullptr;
        if (net->o.tls) {
            net->sock.tls = new CTls;
            if (!net->sock.tls->Init(host, net->o.lax)) { Note(net, L"TLS initialisation failed", cPart); return; }
        }
        Note(net, L"Connecting to " + host + (net->o.tls ? L" (TLS)" : L"") + L"...");
        SetState(net, L"Connecting to " + host + L"...");
        if (!net->sock.Create() || (!net->sock.Connect(host, port) && GetLastError() != WSAEWOULDBLOCK))
            Note(net, L"Connect failed", cPart);
    }
    void ShowNickMenu(CChatWnd* c, const CString& nick, CPoint pt) {   // right-click a nick in the user list
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
                if (d.DoModal() == IDOK && !txt.IsEmpty()) { Send(net, L"NOTICE " + nick + L" :" + txt); Note(net, L"-> -" + nick + L"- " + txt, cNote); }
                break;
            }
        }
    }

    // ---- user input ----
    void OnInput(CChatWnd* w, CString s) {
        Net* net = w->net;
        if (s[0] != L'/' || s.Left(2) == L"//") {
            if (s.Left(2) == L"//") s = s.Mid(1);
            if (w->m_name == L"*status*") Note(net, L"You're not in a channel or query.", cPart);
            else Say(net, w->m_name, s);
            return;
        }
        CString arg = s.Mid(1), cmd = Word(arg); cmd.MakeLower();
        bool inChat = w->m_name != L"*status*";
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
        else if (cmd == L"part" || cmd == L"leave") Send(net, L"PART " + (arg.IsEmpty() && w->m_chan ? w->m_name : arg));
        else if (cmd == L"msg" || cmd == L"m") { CString t = Word(arg); Say(net, t, arg); }
        else if (cmd == L"query" || cmd == L"q") { CString t = Word(arg); Open(net, t, false); if (!arg.IsEmpty()) Say(net, t, arg); }
        else if (cmd == L"me" && inChat) Say(net, w->m_name, arg, true);
        else if (cmd == L"notice") { CString t = Word(arg); Send(net, L"NOTICE " + t + L" :" + arg); Note(net, L"-> -" + t + L"- " + arg, cNote); }
        else if (cmd == L"topic" && w->m_chan) Send(net, arg.IsEmpty() ? L"TOPIC " + w->m_name : L"TOPIC " + w->m_name + L" :" + arg);
        else if (cmd == L"quit") { Send(net, L"QUIT :" + (arg.IsEmpty() ? CString(VERSION) : arg)); net->conn = false; net->sock.Close(); SetState(net, L"Disconnected"); }
        else if (cmd == L"clear") w->Clear();
        else if (cmd == L"echo") { Note(net, arg); }
        else if (cmd == L"raw" || cmd == L"quote") Send(net, arg);
        else if (cmd == L"help") Note(net, L"/server [-m] host [+port = TLS] (-m connects a second, independent network) /nick /join /part /msg /query /me /notice /topic /quit /clear /raw; other /cmds (mode, kick, whois, list...) go to the server as-is");
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

        if (cmd == L"PING") { Send(net, L"PONG :" + P(0)); }
        else if (cmd == L"PRIVMSG" || cmd == L"NOTICE") {
            CString tgt = P(0), txt = P(1); bool notice = cmd == L"NOTICE";
            bool priv = tgt.CompareNoCase(net->nick) == 0;
            CChatWnd* w = (notice && (priv || !Find(net, tgt))) ? Status(net) : (priv ? OpenBg(net, nick) : Open(net, tgt, IsChan(tgt)));
            if (!txt.IsEmpty() && txt[0] == 1) {
                txt.Trim(CString(wchar_t(1)));
                if (txt.Left(6) == L"ACTION") Show(w, L"* " + nick + txt.Mid(6), cAct);
                else if (txt == L"VERSION" && !notice) Send(net, L"NOTICE " + nick + L" :" + CString(wchar_t(1)) + L"VERSION " + CString(VERSION) + CString(wchar_t(1)));
                else Show(w, L"[CTCP " + txt + L" from " + nick + L"]", cNote);
            }
            else if (notice) Show(w, L"-" + (nick.IsEmpty() ? prefix : nick) + L"- " + txt, cNote);
            else Show(w, L"<" + nick + L"> " + txt);
        }
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
            if (P(1).CompareNoCase(net->nick) == 0) { Note(net, L"You were kicked from " + P(0) + L" by " + nick + L" (" + P(2) + L")", cPart); Drop(net, P(0)); return; }
            if (CChatWnd* w = Find(net, P(0))) { w->DelNick(P(1)); Show(w, L"* " + P(1) + L" was kicked by " + nick + L" (" + P(2) + L")", cPart); }
        }
        else if (cmd == L"QUIT") {
            for (auto& kv : m_w) {
                CChatWnd* w = kv.second; if (w->net != net) continue;
                if (w->DelNick(nick) || (!w->m_chan && w->m_name.CompareNoCase(nick) == 0))
                    Show(w, L"* " + nick + L" has quit (" + P(0) + L")", cPart);
            }
        }
        else if (cmd == L"NICK") {
            CString nn = P(0); if (me) net->nick = nn;
            for (auto& kv : m_w) {
                CChatWnd* w = kv.second; if (w->net != net) continue;
                if (w->DelNick(nick)) { w->AddNick(nn); Show(w, L"* " + nick + L" is now known as " + nn, cInfo); }
            }
        }
        else if (cmd == L"TOPIC") {
            if (CChatWnd* w = Find(net, P(0))) { w->SetTopic(P(1)); Show(w, L"* " + nick + L" changed the topic to: " + P(1), cInfo); }
        }
        else if (cmd == L"MODE") {
            CChatWnd* w = Find(net, P(0)); CString m; for (size_t i = 1; i < p.size(); i++) m += p[i] + L" ";
            Show(w ? w : Status(net), L"* " + nick + L" sets mode " + m, cInfo);
            if (w && p.size() > 2) { w->m_refresh = true; Send(net, L"NAMES " + P(0)); }
        }
        else if (cmd == L"001") { net->nick = P(0); Note(net, P(1), cInfo); SetState(net, L"Connected: " + (prefix.IsEmpty() ? net->o.host : prefix) + (net->o.tls ? L" (TLS)" : L""));
            if (!net->o.autojoin.IsEmpty()) Send(net, L"JOIN " + net->o.autojoin); }
        else if (cmd == L"332") { if (CChatWnd* w = Find(net, P(1))) { w->SetTopic(P(2)); Show(w, L"* Topic: " + P(2), cInfo); } }
        else if (cmd == L"353") {
            if (CChatWnd* w = Find(net, P(2))) {
                if (w->m_refresh) { w->ClearNicks(); w->m_refresh = false; }
                int q = 0; CString names = P(3);
                for (CString n = names.Tokenize(L" ", q); !n.IsEmpty(); n = names.Tokenize(L" ", q)) w->AddNick(n);
            }
        }
        else if (cmd == L"366") {}
        else if (cmd == L"433") { net->nick += L"_"; Note(net, L"Nickname in use, trying " + net->nick, cPart); Send(net, L"NICK " + net->nick); }
        else { CString j; for (size_t i = 1; i < p.size(); i++) j += p[i] + L" "; Note(net, j.IsEmpty() ? raw : j, cText); }
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
        auto* a = static_cast<CChatWnd*>(MDIGetActive());
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
        auto* a = static_cast<CChatWnd*>(MDIGetActive());
        Net* net = a ? a->net : nullptr;
        if (!net || !net->conn) { AfxMessageBox(L"Connect to a server first, then use Channel Favorites to join."); return; }
        Send(net, L"JOIN " + e.chan + (e.key.IsEmpty() ? CString() : L" " + e.key));
    }

    void BuildToolbar() {   // real icons from the optional resource bitmap; falls back to plain drawn glyphs if MiniIRC.rc wasn't linked in
        const int N = 6;
        CBitmap resBmp;
        bool haveRes = resBmp.LoadBitmap(102) != 0;   // id 102 in MiniIRC.rc ("toolbar.bmp"); absent in the plain one-file build
        int W = haveRes ? 24 : 16, H = W;
        m_tbImg.Create(W, H, ILC_COLOR24 | ILC_MASK, N, 0);
        if (haveRes) {
            m_tbImg.Add(&resBmp, RGB(255, 0, 255));   // strip order: connect, disconnect, server list, cascade, tile, help
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
            mem.SelectObject(oldBmp);
            m_tbImg.Add(&bmp, RGB(255, 0, 255));
        }
        m_tb.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS);
        m_tb.GetToolBarCtrl().SetImageList(&m_tbImg);
        TBBUTTON b[10] = {};
        b[0].iBitmap = 0; b[0].idCommand = IDM_CONNECT; b[0].fsState = TBSTATE_ENABLED; b[0].fsStyle = TBSTYLE_BUTTON;
        b[1].iBitmap = 1; b[1].idCommand = IDM_DISCONNECT; b[1].fsState = TBSTATE_ENABLED; b[1].fsStyle = TBSTYLE_BUTTON;
        b[2].fsStyle = TBSTYLE_SEP;
        b[3].iBitmap = 2; b[3].idCommand = IDM_SERVERS; b[3].fsState = TBSTATE_ENABLED; b[3].fsStyle = TBSTYLE_BUTTON;
        b[4].fsStyle = TBSTYLE_SEP;
        b[5].iBitmap = 3; b[5].idCommand = IDM_CASCADE; b[5].fsState = TBSTATE_ENABLED; b[5].fsStyle = TBSTYLE_BUTTON;
        b[6].iBitmap = 4; b[6].idCommand = IDM_TILE; b[6].fsState = TBSTATE_ENABLED; b[6].fsStyle = TBSTYLE_BUTTON;
        b[7].fsStyle = TBSTYLE_SEP;
        b[8].iBitmap = 5; b[8].idCommand = IDM_ABOUT; b[8].fsState = TBSTATE_ENABLED; b[8].fsStyle = TBSTYLE_BUTTON;
        b[9].fsStyle = TBSTYLE_SEP;
        m_tb.GetToolBarCtrl().AddButtons(10, b);
        m_tb.GetToolBarCtrl().SetButtonSize(haveRes ? CSize(36, 34) : CSize(28, 26));
    }
    afx_msg void OnConnectDlg() {   // reuses the active window's network if it's idle/disconnected; otherwise adds a new one (like /server -m)
        auto* a = static_cast<CChatWnd*>(MDIGetActive());
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
        auto* a = static_cast<CChatWnd*>(MDIGetActive());
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
    afx_msg void OnTimer(UINT_PTR) { if (m_menuOpen) return; RefreshBars(); CheckLayout(); }
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
            CMenu* f = m_menu.GetSubMenu(0); CMenu* w = m_menu.GetSubMenu(1); CMenu* hp = m_menu.GetSubMenu(2);
            bool ours = (f && f->GetSafeHmenu() == h) || (w && w->GetSafeHmenu() == h) || (hp && hp->GetSafeHmenu() == h);
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
        CMenu f, s, c, w, h;
        f.CreatePopupMenu(); 
        f.AppendMenu(MF_STRING, IDM_CONNECT, L"&Connect..."); 
        f.AppendMenu(MF_STRING, IDM_DISCONNECT, L"&Disconnect");
        f.AppendMenu(MF_SEPARATOR); 
        f.AppendMenu(MF_STRING, IDM_FONT, L"&Font...");
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
        h.AppendMenu(MF_STRING, IDM_ABOUT, L"&About");
        m_menu.CreateMenu();
        m_menu.AppendMenu(MF_POPUP, (UINT_PTR)f.Detach(), L"&File");
        m_menu.AppendMenu(MF_STRING, IDM_SERVERS, L"&Servers");
        m_menu.AppendMenu(MF_STRING, IDM_CHANFAVS, L"&Favorites");
        m_menu.AppendMenu(MF_POPUP, (UINT_PTR)w.Detach(), L"&Window");
		m_menu.AppendMenu(MF_POPUP, (UINT_PTR)h.Detach(), L"&Help");   
        SetMenu(&m_menu); DrawMenuBar();
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
            SetForegroundWindow();   // required by Windows for the popup to reliably receive clicks at all
            m_menuOpen = true;
            int r = m.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, this);
            m_menuOpen = false;
            PostMessage(WM_NULL, 0, 0);   // MSDN-documented pairing for the above; without it the window can be left in a bad activation state
            if (r == 1) SetSwPos(true); else if (r == 2) SetSwPos(false);
        };
        m_bar.onChan = [this](CString c) {   // clicking a channel name in the status bar's "Channels:" pane
            auto* a = static_cast<CChatWnd*>(MDIGetActive());
            if (a && a->net) Goto(a->net, c);
        };
        m_sw.onSel = [this](int i) {   // click a switchbar button -> activate that window
            if (i >= 0 && i < (int)m_tabWnds.size()) Activate(m_tabWnds[i]);
        };
        m_sw.onMenu = [this](int i, CPoint pt) {   // right-click a switchbar button
            if (i < 0 || i >= (int)m_tabWnds.size()) return;
            CChatWnd* w = m_tabWnds[i];
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
            if (i < 0 || i >= (int)m_tabWnds.size() || m_tabWnds[i]->m_name == L"*status*") return;
            m_tabWnds[i]->PostMessage(WM_CLOSE);
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
    ON_COMMAND(IDM_CONNECT, OnConnectDlg) 
    ON_COMMAND(IDM_DISCONNECT, OnDisconnect)
    ON_COMMAND(IDM_CASCADE, OnCascade) 
    ON_COMMAND(IDM_TILE, OnTile) 
    ON_COMMAND(IDM_EXIT, OnExit) 
    ON_COMMAND(IDM_FONT, OnFont) 
    ON_COMMAND(IDM_SERVERS, OnServerList) 
    ON_COMMAND(IDM_CHANFAVS, OnChanFavs) 
	ON_COMMAND(IDM_ABOUT, OnAbout)
    ON_WM_TIMER() 
    ON_WM_SIZE()
    ON_COMMAND(IDM_SWTOP, OnSwTop) 
    ON_COMMAND(IDM_SWBOTTOM, OnSwBottom)
    ON_UPDATE_COMMAND_UI(IDM_SWTOP, OnUpdateSwTop) ON_UPDATE_COMMAND_UI(IDM_SWBOTTOM, OnUpdateSwBottom) ON_WM_INITMENUPOPUP()
END_MESSAGE_MAP()

class CIRCClientApp : public CWinApp {
public:
    BOOL InitInstance() override {
        CWinApp::InitInstance();
        
        AfxSocketInit(); 
        AfxInitRichEdit2();
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
} theApp;
