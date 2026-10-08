alias Tetris {
  Tetris:Cleanup
  window -Bdfhnpl20 +d @Tetris:Buffer -1 -1 376 454
  if (!$isfile($scriptdirTetris.bmp)) { Tetris:MakeSprites }
  drawpic -c @Tetris:Buffer 0 0 $qt($scriptdirTetris.bmp)
  var %x 1 | while (%x < 26) { aline -l @Tetris:Buffer 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 | inc %x }
  window -nBdfpk0C +d @Tetris -1 -1 360 454
  set -e %Tetris:State Play
  Tetris:Shape
  window -a @Tetris
  Tetris:Play
}

; ---- Debug toggle -------------------------------------------------------------------------------------------
; Set %Tetris:Debug to 1 (e.g. "//set -e %Tetris:Debug 1" from the status window) to turn on diagnostic output;
; leave at 0 (the default) for normal play. Call Tetris:Dbg exactly like /echo -s -- it no-ops when debug is off,
; so diagnostic lines can be added/left in place anywhere in this script without ever spamming a normal game.
;   Tetris:Dbg DIAG4 x=%x sL=%sL sP=%sP nL=%nL
set -e %Tetris:Debug 0
alias -l Tetris:Dbg { if (%Tetris:Debug) echo -s $1- }
; --------------------------------------------------------------------------------------------------------------

alias -l Tetris:Cleanup { .TimerTetris Off | unset %Tetris:* | close -@ @Tetris* }
alias -l Tetris:Shape {
  :loop
  set -e %Tetris:Shape %Tetris:nShape
  if ($r(1,7) == 1) { set %Tetris:nShape r 1 0 8 -1 8 -2 8 -3 8 }
  if ($v1 == 2) { set %Tetris:nShape g 1 0 8 0 7 -1 7 -1 8 }
  if ($v1 == 3) { set %Tetris:nShape b 1 0 8 0 7 -1 8 -1 9 }
  if ($v1 == 4) { set %Tetris:nShape o 1 0 8 0 9 -1 7 -1 8 }
  if ($v1 == 5) { set %Tetris:nShape y 1 0 8 -1 8 0 9 0 7 }
  if ($v1 == 6) { set %Tetris:nShape p 1 0 8 -1 8 -2 8 -2 7 }
  if ($v1 == 7) { set %Tetris:nShape t 1 0 8 -1 8 -2 8 -2 9 }
  if (!%Tetris:Shape) { set -e %Tetris:Shape %Tetris:nShape | goto loop }
  Tetris:Land
}
alias -l Tetris:Land {
  tokenize 32 %Tetris:Shape
  while ($3 < 25) && (!$gettok($line(@Tetris:Buffer,$calc($3 +1),1),$4,32) && !$gettok($line(@Tetris:Buffer,$calc($5 +1),1),$6,32) && !$gettok($line(@Tetris:Buffer,$calc($7 +1),1),$8,32) && !$gettok($line(@Tetris:Buffer,$calc($9 +1),1),$10,32)) { tokenize 32 $1-2 $calc($3 + 1) $4 $calc($5 +1) $6 $calc($7 + 1) $8 $calc($9 + 1) $10 }
  set -e %Tetris:Land $1-
}
alias -l Tetris:Redraw {
  if (!$window(@Tetris) || !$window(@Tetris:Buffer)) { Tetris:Cleanup }
  drawcopy -n @Tetris:Buffer 0 0 360 454 @Tetris 0 0
  var %x = 25, %c
  while ((%x && $line(@Tetris:Buffer,%x,1) != 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)) {
    tokenize 32 $v1
    var %y = $calc((%x -1) * 17 + 23)
    if ($1) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$1) - 1)) 16 16 @Tetris 7 %y }
    if ($2) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$2) - 1)) 16 16 @Tetris 24 %y }
    if ($3) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$3) - 1)) 16 16 @Tetris 41 %y }
    if ($4) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$4) - 1)) 16 16 @Tetris 58 %y }
    if ($5) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$5) - 1)) 16 16 @Tetris 75 %y }
    if ($6) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$6) - 1)) 16 16 @Tetris 92 %y }
    if ($7) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$7) - 1)) 16 16 @Tetris 109 %y }
    if ($8) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$8) - 1)) 16 16 @Tetris 126 %y }
    if ($9) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$9) - 1)) 16 16 @Tetris 143 %y }
    if ($10) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$10) - 1)) 16 16 @Tetris 160 %y }
    if ($11) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$11) - 1)) 16 16 @Tetris 177 %y }
    if ($12) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$12) - 1)) 16 16 @Tetris 194 %y }
    if ($13) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$13) - 1)) 16 16 @Tetris 211 %y }
    if ($14) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$14) - 1)) 16 16 @Tetris 228 %y }
    if ($15) { drawcopy -n @Tetris:Buffer 360 $calc(16 * ($pos(rgboypt,$15) - 1)) 16 16 @Tetris 245 %y }
    dec %x
  }
  tokenize 32 %Tetris:nShape
  %c = $calc(16 * ($pos(rgboypt,$1) - 1))
  drawcopy -n @Tetris:Buffer 360 %c 16 16 @Tetris $calc(($4 -8)*17+294) $calc(($3 +3)*17+371)
  drawcopy -n @Tetris:Buffer 360 %c 16 16 @Tetris $calc(($6 -8)*17+294) $calc(($5 +3)*17+371)
  drawcopy -n @Tetris:Buffer 360 %c 16 16 @Tetris $calc(($8 -8)*17+294) $calc(($7 +3)*17+371)
  drawcopy -n @Tetris:Buffer 360 %c 16 16 @Tetris $calc(($10 -8)*17+294) $calc(($9 +3)*17+371)
  var %lv = $floor($calc($iif(%Tetris:Lines,$v1,0) / 10))
  var %ln = $iif($calc(10 - %Tetris:Lines % 10),$v1,10)
  var %sc = $iif(%Tetris:Score,$v1,0)
  drawtext -rno @Tetris $rgb(128,128,128) Tahoma 12 $calc(348 - $width(%lv,Tahoma,12,1)) 50 %lv
  drawtext -rno @Tetris $rgb(128,128,128) Tahoma 12 $calc(348 - $width(%ln,Tahoma,12,1)) 80 %ln
  drawtext -rno @Tetris $rgb(128,128,128) Tahoma 12 $calc(348 - $width(%sc,Tahoma,12,1)) 110 %sc
  if (%Tetris:Land && !%TetrisHideGhost) {
    tokenize 32 %Tetris:Land
    var %c = $gettok(255 65280 16711680 33023 65535 16711935 16776960,$pos(rgboypt,$1),32)
    if ($3 > 0) drawrect -rn @Tetris %c 2 $calc($4 *17 - 10) $calc($3 *17 + 6) 16 16
    if ($5 > 0) drawrect -rn @Tetris %c 2 $calc($6 *17 - 10) $calc($5 *17 + 6) 16 16
    if ($7 > 0) drawrect -rn @Tetris %c 2 $calc($8 *17 - 10) $calc($7 *17 + 6) 16 16
    if ($9 > 0) drawrect -rn @Tetris %c 2 $calc($10 *17 - 10) $calc($9 *17 + 6) 16 16
  }
  tokenize 32 %Tetris:Shape
  %c = $calc(16 * ($pos(rgboypt,$1) - 1))
  if ($3 > 0) drawcopy -n @Tetris:Buffer 360 %c 16 16 @Tetris $calc(-10 + $4 * 17) $calc($3 * 17 + 6)
  if ($5 > 0) drawcopy -n @Tetris:Buffer 360 %c 16 16 @Tetris $calc(-10 + $6 * 17) $calc($5 * 17 + 6)
  if ($7 > 0) drawcopy -n @Tetris:Buffer 360 %c 16 16 @Tetris $calc(-10 + $8 * 17) $calc($7 * 17 + 6)
  if ($9 > 0) drawcopy -n @Tetris:Buffer 360 %c 16 16 @Tetris $calc(-10 + $10 * 17) $calc($9 * 17 + 6)
  if (%Tetris:State == GAMEOVER) || (%Tetris:State == PAUSED) {
    var %m = $v1
    drawrect -rfn @Tetris $rgb(96,96,96) 1 34 185 200 100
    drawrect -rn @Tetris $rgb(192,192,192) 1 34 185 200 100
    drawtext -rno @Tetris 0 "Courier New" 40 $calc(34 + (200 - $width(%m,Courier New,40,1)) /2) $calc(185 + (100 - $height(%m,Courier New,40)) / 2) %m
  }
  drawdot @Tetris
}
alias -l Tetris:MakeSprites {
  if ($isfile($scriptdirTetris.bmp)) { .remove $qt($scriptdirTetris.bmp) }
  var %x = 1, %c = 255,000,000 000,255,000 000,000,255 255,128,000 255,255,000 255,000,255 000,255,255
  if ($window(@Tetris:MakeSprites)) { window -c $v1 }
  window -Bdfhnp +d @Tetris:MakeSprites -1 -1 376 454
  drawrect -nfr @Tetris:MakeSprites 0 1 0 0 360 454
  drawrect -nfr @Tetris:MakeSprites $rgb(128,128,128) 1 1 1 358 15
  drawrect -nrf @Tetris:MakeSprites $rgb(32,32,32) 1 7 23 254 424 268 39 85 90
  drawrect -nfr @Tetris:MakeSprites $rgb(96,96,96) 1 268 346 85 15 268 23 85 15
  drawrect -nr @Tetris:MakeSprites $rgb(192,192,192) 1 0 0 360 454 6 22 256 426 267 345 87 103 267 22 86 108
  drawline -nr @Tetris:MakeSprites $rgb(192,192,192) 1 0 16 360 16
  drawline -nr @Tetris:MakeSprites $rgb(192,192,192) 1 267 361 353 361
  drawline -nr @Tetris:MakeSprites $rgb(192,192,192) 1 267 38 353 38
  drawline -nr @Tetris:MakeSprites $rgb(192,192,192) 1 5 12 5 3 14 3 13 4 6 4 6 11 5 12
  drawline -nr @Tetris:MakeSprites $rgb(64,64,64) 1 15 4 15 13 6 13 7 12 14 12 14 5 15 4
  drawrect -nr @Tetris:MakeSprites $rgb(192,192,192) 1 55 2 10 2 55 12 6 2 57 4 2 8 63 4 2 2 62 8 2 6 64 12 2 2 67 2 6 2 67 12 6 2 69 4 2 8 75 4 2 2 83 4 2 2 75 10 2 2 83 10 2 2 75 2 10 2 75 12 10 2 331 10 8 2 19 2 2 4 27 2 2 4 21 2 6 2 23 5 2 7 21 12 6 2 31 2 10 2 31 12 10 2 33 4 2 8 36 7 2 2 39 4 2 2 39 10 2 2 43 2 2 4 51 2 2 4 45 2 6 2 47 5 2 7 45 12 6 2
  drawline -nr @Tetris:MakeSprites $rgb(192,192,192) 1 63 5 58 10 58 11 64 5 63 5
  drawline -nr @Tetris:MakeSprites $rgb(192,192,192) 1 75 5 83 10 84 10 76 5 75 5
  drawline -nr @Tetris:MakeSprites $rgb(192,192,192) 1 345 5 351 11 352 11 346 5 345 5
  drawline -nr @Tetris:MakeSprites $rgb(192,192,192) 1 351 5 345 11 346 11 352 5 351 5
  while (%x < 8) {
    tokenize 44 $gettok(%c,%x,32)
    var %nY = $calc(2 + 16 * (%x - 1)), %nY-2 = $calc(%nY -2), %nY-1 = $calc(%nY -1), %nY+1 = $calc(%nY +11), %nY+2 = $calc(%nY +12), %nY+3 = $calc(%nY +13)
    drawline -rn @Tetris:MakeSprites %tc 1 362 %nY-1 373 %nY-1 374 %nY-2 361 %nY-2 362 %nY-1
    drawline -rn @Tetris:MakeSprites %bc 1 362 %nY+2 373 %nY+2 374 %nY+3 361 %nY+3 362 %nY+2
    drawline -rn @Tetris:MakeSprites %sc 1 361 %nY 361 %nY+1 360 %nY+2 360 %nY-1 361 %nY
    drawline -rn @Tetris:MakeSprites %sc 1 374 %nY 374 %nY+1 375 %nY+2 375 %nY-1 374 %nY
    drawdot -rn @Tetris:MakeSprites 1 360 %nY-2 361 %nY-1 374 %nY-1 375 %nY-2
    drawdot -rn @Tetris:MakeSprites 1 360 %nY+3 361 %nY+2 374 %nY+2 375 %nY+3
    drawrect -fnr @Tetris:MakeSprites $rgb($1,$2,$3) 1 362 %nY 12 12
    inc %x
  }
  %x = 7 | while (%x < 41) { drawdot -rn @Tetris:MakeSprites 4210752 1 %x $iif($calc(%x % 2),56,39) | inc %x }
  %x = 23 | while (%x < 57) { drawdot -rn @Tetris:MakeSprites 4210752 1 $iif($calc(%x % 2),40,23) %x | inc %x }
  %x = 1 | while (%x < 12) { drawcopy -n @Tetris:MakeSprites 7 23 34 $iif(%x == 1,34,50) @Tetris:MakeSprites 7 $calc(23 + 34 * %x) | inc %x }
  %x = 1 | while (%x < 7) { drawcopy -n @Tetris:MakeSprites 7 23 $iif(%x == 1,34,50) 424 @Tetris:MakeSprites $calc(7 + 34 * %x) 23 | inc %x }
  drawcopy @Tetris:MakeSprites 15 31 85 85 @Tetris:MakeSprites 268 362
  drawtext -onr @Tetris:MakeSprites $rgb(192,192,192) Tahoma 12 271 23 Stats
  drawtext -onr @Tetris:MakeSprites $rgb(128,128,128) Tahoma 12 274 39 Level:
  drawtext -onr @Tetris:MakeSprites $rgb(128,128,128) Tahoma 12 274 69 Lines Left:
  drawtext -onr @Tetris:MakeSprites $rgb(128,128,128) Tahoma 12 274 99 Score:
  drawtext -onr @Tetris:MakeSprites $rgb(192,192,192) Tahoma 12 271 346 Next
  drawdot @Tetris:MakeSprites
  drawsave @Tetris:MakeSprites $qt($scriptdirTetris.bmp)
  window -c @Tetris:MakeSprites
  :error
  if ($window(@Tetris:MakeSprites)) { window -c @Tetris:MakeSprites }
}

on *:CLOSE:@Tetris,@Tetris:{ Tetris:Cleanup }

alias Tetris:Play {
  if ($window(@Tetris) && $window(@Tetris:Buffer)) {
    if (%Tetris:State == Play) {
      :reCheck
      tokenize 32 %Tetris:Shape
      var %s, %sL, %sP, %nL, %x = 3, %ok, %nShape $1-2
      while (%x < 11) {
        var %sL = $(,$ $+ %x), %sP = $(,$ $+ $calc(%x +1)), %nL = $calc(%sL +1)
        Tetris:Dbg DIAG4 x=%x sL=%sL sP=%sP nL=%nL
        if (%sL == 0) && ($gettok($line(@Tetris:Buffer,1,1),%sP,32) != 0) { set -e %Tetris:State GAMEOVER | break }
        elseif (%sL == 25) || (%sL isnum 0-24 && $gettok($line(@Tetris:Buffer,%nL,1),%sP,32) != 0) { break }
        else { %nShape = %nShape %nL %sP }
        inc %x 2
      }
      if (%Tetris:State == PLAY) {
        if ($numtok(%nShape,32) != 10) || ($v1 == 10 && 25 isin %nShape) {
          if ($numtok(%nShape,32) == 10 && 25 isin %nShape) { tokenize 32 $v2 }
          if ($3 < 1 || $4 < 1 || $7 < 1 || $9 < 1) { %Tetris:State = GAMEOVER | goto end }
          rline -l @Tetris:Buffer $3 $puttok($line(@Tetris:Buffer,$3,1),$1,$4,32)
          rline -l @Tetris:Buffer $5 $puttok($line(@Tetris:Buffer,$5,1),$1,$6,32)
          rline -l @Tetris:Buffer $7 $puttok($line(@Tetris:Buffer,$7,1),$1,$8,32)
          rline -l @Tetris:Buffer $9 $puttok($line(@Tetris:Buffer,$9,1),$1,$10,32)
          var %lns
          while ($fline(@Tetris:Buffer,/^([^0](?:\x20|$)){15}$/,1,3)) {
            dline -l @Tetris:Buffer $v1
            iline -l @Tetris:Buffer 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            inc %lns
          }
          if (%lns) { inc -e %Tetris:Lines %lns | inc -e %Tetris:Score $floor($calc(($floor($calc(%Tetris:Lines / 10)) +1) *10 * (1.2^ %lns))) }
          Tetris:Shape
          goto reCheck
        }
        else {
          set -e %Tetris:Shape %nShape
          tokenize 32 %nShape
          if ($3 < 1 && $5 < 1 && $7 < 1 && $8 < 1) { goto reCheck }
        }
      }
    }
    :end
    Tetris:Redraw
    .timerTetris -h 1 $iif($iif(%Tetris:State != Play,0,$floor($calc(%Tetris:Lines / 10))) > 19,50,$floor($calc(50 + 1.435 ^(20 - $v1)))) Tetris:Play
  }
  else { Tetris:Cleanup }
}

Menu @Tetris {
  $iif(!%TetrisHideGhost,$style(1)) Show Ghosts:{ set -e %TetrisHideGhost $iif(%TetrisHideGhost,$false,$true) | Tetris:Redraw }
  Restart:{ Tetris }
  -
  Close:{ Tetris:Cleanup }
}
Menu @Tetris {
  sclick:{
    if (%Tetris:State == PAUSED) { set -e %Tetris:State PLAY | Tetris:Play }
    elseif ($inrect($mouse.x,$Mouse.y,329,3,12,11)) { set -e %Tetris:State PAUSED | window -n @Tetris }
    elseif ($inrect($mouse.x,$mouse.y,344,3,12,11)) { Tetris:Cleanup }
    elseif (%Tetris:State == PLAY) { set -e %Tetris:State PAUSED | Tetris:Redraw }
    elseif (%Tetris:State == GAMEOVER) { Tetris }
  }
}

on *:KEYDOWN:@Tetris:*:{
  if (!$window(@Tetris:Buffer)) { Tetris:Cleanup }
  elseif (%Tetris:State == GAMEOVER) { Tetris }
  elseif (%Tetris:State == PAUSED) { set -e %Tetris:State PLAY | Tetris:Redraw }
  elseif (%Tetris:State == PLAY) {
    if ($keyval == 32) {
      tokenize 32 %Tetris:Land
      set -e %Tetris:Shape $1-
      Tetris:Play
    }
    elseif ($keyval == 40 || $keyval == 100) && (%Tetris:Shape != %Tetris:Land) {
      tokenize 32 $v1
      set -e %Tetris:Shape $1-2 $calc($3 +1) $4 $calc($5 + 1) $6 $calc($7 +1) $8 $calc($9 +1) $10
      Tetris:Redraw
    }
    elseif ($istok(37 39 97 100,$Keyval,32)) {
      tokenize 32 %Tetris:Shape
      var %n = $iif($keyval isin 3797,-,+), %n1 = $calc($4 %n 1), %n2 = $calc($6 %n 1), %n3 = $calc($8 %n 1), %n4 = $calc($10 %n 1)
      if (%n1 !isnum 1-15 || %n2 !isnum 1-15 || %n3 !isnum 1-15 || %n4 !isnum 1-15) { return }
      if ($gettok($line(@Tetris:Buffer,$3,1),%n1,32)) { return }
      if ($gettok($line(@Tetris:Buffer,$5,1),%n2,32)) { return }
      if ($gettok($line(@Tetris:Buffer,$7,1),%n3,32)) { return }
      if ($gettok($line(@Tetris:Buffer,$9,1),%n4,32)) { return }
      set -e %Tetris:Shape $1-3 %n1 $5 %n2 $7 %n3 $9 %n4
      Tetris:Land
      Tetris:Redraw
    }
    elseif ($keyval == 38 || $keyval == 119) {
      tokenize 32 %Tetris:Shape
      var %sL = $3
      var %sP = $4
      var %nShape
      if (!$regex($1,/^[rboypt]$/)) { return }
      elseif ($1 == r) {
        if ($2 == 1) && ($tetris:Check(%sL %sP  %sL $calc(%sP +1) %sL $calc(%sP +2) %sL $calc(%sP -1))) { %nShape = $1 2 $v1 }
        elseif ($2 == 2) && ($tetris:Check(%sL %sP $calc(%sL -1) %sP $calc(%sL -2) %sP $calc(%sL -3) %sP)) { %nShape = $1 1 $v1 }
        else { return }
      }
      elseif ($1 == b) {
        if ($2 == 1) && ($tetris:Check(%sL %sP $calc(%sL -1) %sp $calc(%sL -1) $calc(%sp -1) $calc(%sL -2) $calc(%sP -1))) { %nShape = $1 2 $v1 }
        elseif ($2 == 2) && ($tetris:Check(%sL %sp %sL $calc(%sP -1) $calc(%sL -1) %sp $calc(%sL -1) $calc(%sp +1))) { %nShape = $1 1 $v1 }
        else { return }
      }
      elseif ($1 == o) {
        if ($2 == 1) && ($tetris:Check(%sL %sP, $calc(%sL -1) %sp,$calc(%sL -1) $calc(%sp +1), $calc(%sL -2) $calc(%sP +1))) { %nShape = $1 2 $v1 }
        elseif ($2 == 2) && ($tetris:Check( %sL %sP, %sL $calc(%sP +1), $calc(%sL -1) %sp, $calc(%sL -1) $calc(%sp -1))) { %nShape = $1 1 $v1 }
        else { return }
      }
      elseif ($1 == y) {
        if ($2 == 1) && ($Tetris:Check($calc(%sL -2) %sP $calc(%sL -1) $calc(%sP +1))) { %nShape = $1 2 $3-6 $v1 }
        elseif ($2 == 2) && ($Tetris:Check($calc(%sL -1) $calc(%sP -1) $calc(%sL -1) $calc(%sp +1))) { %nShape = $1 3 $3-6 $v1 }
        elseif ($2 == 3) && ($Tetris:Check($calc(%sL -2) %sp $calc(%sL -1) $calc(%sp -1))) { %nShape = $1 4 $3-6 $v1 }
        elseif ($2 == 4) && ($tetris:Check(%sl $calc(%sP -1) %sL $calc(%sP +1))) { %nShape = $1 1 $3-6 $v1 }
        else { return }
      }
      elseif ($1 == p) {
        if ($2 == 1) && ($Tetris:Check(%sL $calc(%sP -1) %sL $calc(%sP +1) $calc(%sL -1) $calc(%sP +1))) { %nShape = $1 2 $3-4 $v1 }
        elseif ($2 == 2) && ($Tetris:Check(%sL $calc(%sp +1) $calc(%sL -1) %sp $calc(%sL -2) %sp)) { %nShape = $1 3 $3-4 $v1 }
        elseif ($2 == 3) && ($Tetris:Check(%sL $calc(%sp -1) $calc(%sl -1) $calc(%sp -1) $calc(%sL -1) %sp,$calc(%sL -1) $calc(%sP +1))) { %nShape = $1 4 $v1 }
        elseif ($2 == 4) && ($Tetris:Check(%sL $calc(%sp +1) $calc(%sL -1) $calc(%sp +1) $calc(%sL -2) $calc(%sP +1)  $calc(%sL -2) %sP)) { %nShape = $1 1 $v1 }
        else { return }
      }
      elseif ($1 == t) {
        if ($2 == 1) && ($Tetris:Check(%sL $calc(%sP +1) $calc(%sL -1) $calc(%sP +1) $calc(%sL -1) %sp $calc(%sL -1) $calc(%sP -1))) { %nShape = $1 2 $v1 }
        elseif ($2 == 2) && ($Tetris:Check(%sL $calc(%sp -1) %sL $calc(%sp -2) $calc(%sL -1) $calc(%sP -1) $calc(%sL -2) $calc(%sP -1))) { %nShape = $1 3 $v1 }
        elseif ($2 == 3) && ($Tetris:Check(%sL $calc(%sP +1) %sL $calc(%sP -1) $calc(%sl -1) $calc(%sP -1))) { %nShape = $1 4 $3-4 $v1 }
        elseif ($2 == 4) && ($Tetris:Check($calc(%sL -1) %sp $calc(%sL -2) %sp  $calc(%sL -2) $calc(%sP +1))) { %nShape = $1 1 $3-4 $v1 }
        else { return }
      }
      set -e %Tetris:Shape %nShape
      Tetris:Land
      Tetris:Redraw
    }
  }
}

alias -l Tetris:Check {
  tokenize 32 $1-
  var %x = 1, %r
  while (%x < $0) {
    var %sL = $(,$ $+ %x)
    var %sP = $(,$ $+ $calc(%x +1))
    if (%sL > 25) || (%sP !isnum 1-15) || ($gettok($line(@Tetris:Buffer,%sL,1),%sP,32)) { return }
    %r = %r %sL %sP
    inc %x 2
  }
  return %r
}
