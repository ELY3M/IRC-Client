; ASCII Chart by fuap@EFNet (ameistad@online.no)

alias ascii dialog -m ascii ascii
dialog ascii {
  title "ASCII Chart"
  size -1 -1 300 400
  option pixels
  tab "32-127", 501, 10 10 280 350
  tab "128-255", 502
  list 503, 20 40 260 300, tab 501
  list 504, 20 40 260 300, tab 502
  text "Doubleclick on an item to copy to clipboard", 505, 10 375 210 20
  button "Close", 605, 225 375 50 20, ok 
}
alias -l _asciimake {
  var %a = 32, %b = 128
  while (%a < 128) { did -a ascii 503 %a $+ : $chr(%a) | inc %a }
  while (%b < 256) { did -a ascii 504 %b $+ ; $chr(%b) | inc %b }
}
on *:dialog:ascii:init:0: _asciimake
on *:dialog:ascii:dclick:503: var %a = $did(ascii,503,$did(ascii,503).sel) | clipboard $gettok(%a,2,32) | editbox $editbox($active) $gettok(%a,2,32) 
on *:dialog:ascii:sclick:504: var %a = $did(ascii,504,$did(ascii,504).sel) | clipboard $gettok(%a,2,32) | editbox $editbox($active) $gettok(%a,2,32) 

menu menubar,query,status,channel {
  -
  -= Ascii Chart =-:ascii
}
