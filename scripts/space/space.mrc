
; IRC Invaders v1.1 - by entropy 2020

;=========================================================== MENUS

menu status {
  -
  $iif(!$window(@IRC),IRC Invaders $sversion)
  .800 x 600 $chr(9) (fast): { %_master = $vol(master) | %_new = $calc($vol(master) / 3) | vol -v %_new | %_display = 800 600 | space }
  .1024 x 768 $chr(9) (medium): { %_master = $vol(master) | %_new = $calc($vol(master) / 3) | vol -v %_new | %_display = 1024 768 | space }
  .Fullscreen $chr(9) (slow): { %_master = $vol(master) | %_new = $calc($vol(master) / 3) | vol -v %_new | %_display = $window(-1).w $window(-1).h | space }
  .-
  .Unload: { .unload -rs $qt($script) }
  -
}

menu @IRC {
  -
  $iif(!$timer(pause) && $timer(space) && !%space_start && !%space_inter,Pause $chr(9) [PAUSE]) { pause }
  $iif($timer(pause) && %space_over != yes && !%space_inter,Unpause $chr(9) [PAUSE]) { unpause }
  -
  Replay  
  .800 x 600 $chr(9) (fast) { vol -v %_new | %_display = 800 600 | space }
  .1024 x 768 $chr(9) (medium) { vol -v %_new | %_display = 1024 768 | space }
  .Fullscreen $chr(9) (slow) { vol -v %_new | %_display = $window(-1).w $window(-1).h | space }  
  -
  Top 10 Scores $chr(9) [F1] { f1 }
  -
  Close { cleanup }
  -
}

;=========================================================== MAIN ALIASES

alias -l sversion { return 1.1 }
alias -l sauthor { return entropy }
alias -l syear { return 2020 }

;=========================================================== ON CLOSE

on *:close:@IRC:{ cleanup }

;=========================================================== LOAD/UNLOAD

on *:load:{
  if ($version < 7.55) { .unload -rs $qt($script) | echo 4 -tag * IRC Invaders requires mIRC 7.55 or greater! }
  else { echo 4 -tag * IRC Invaders - by $sauthor $syear - loaded! }
}
on *:unload:{
  echo 4 -tag * IRC Invaders - by $sauthor $syear - unloaded!
}

;=========================================================== KEYS


on *:playend:{ 
  if ($nopath($filename) == bg.mp3) { .splay -p $qt($scriptdirbg.mp3) }
}

alias -l cleanup {
  .splay stop
  if ($isfile($qt($scriptdirsave.png))) { .remove $qt($scriptdirsave.png) }
  .timerspace* off
  .timerpause off
  hfree space
  window -c @IRC.buf
  window -c @IRC.buf2
  window -c @IRC
  if ($isfile($qt($scriptdirbg.txt))) { .remove $qt($scriptdirbg.txt) }
  vol -v %_master
  unset %space_* %new_* %_dead %_damage %_owner2 %_owner %_new %_master %_display
}

alias -l checkcontrols { ;$1=angle | if ($hget(space,player) && !$istok(%_dead,player,32)) { %space_angle = $addtok($remove(%space_angle,$1),$1,32) } }
alias -l uncheckcontrols { ;$1=angle | if ($hget(space,player) && !$istok(%_dead,player,32)) { %space_angle = $remtok(%space_angle,$1,1,32) } }

on *:keydown:@IRC:37:{ ;Left | checkcontrols left }
on *:keydown:@IRC:38:{ ;Up | checkcontrols up }
on *:keydown:@IRC:39:{ ;Right | checkcontrols right }
on *:keydown:@IRC:40:{ ;Down | checkcontrols down }

on *:keyup:@IRC:37:{ uncheckcontrols left }
on *:keyup:@IRC:38:{ uncheckcontrols up } 
on *:keyup:@IRC:39:{ uncheckcontrols right }
on *:keyup:@IRC:40:{ uncheckcontrols down }

on *:keydown:@IRC:17:{ fire }

on *:keydown:@IRC:19:{ 
  if (%space_win || %space_inter) { return }
  elseif (!$timer(pause) && $timer(space) && !%space_start) { pause }
  elseif ($timer(pause) && %space_over != yes) { unpause }
}

alias -l fire {
  ;Ctrl (fire)
  if (%space_protect || %space_shot == $null || $timer(pause)) { return }
  var %_x = $calc($gettok($hget(space,player),1,32) + 14)
  var %_y = $calc($gettok($hget(space,player),2,32) - 14)
  if (!%space_wait && !$keyrpt) { 
    %space_wait = yes
    .timerspaceshotplayer -om 1 %space_shot unset $+(%,space_wait)
    hadd -m space $+(missles,$r(1000,9999)) %_x %_y missles.png 10 20 10 up 25 20 player 0.8 50
    inc %space_shots
    .splay -w $qt($scriptdirlaser.wav)
  }
}


;=========================================================== ALIASES

alias -l entfilter { var %math = $gettok($2,1,32) - $gettok($1,1,32) | return $calc(%math / $abs(%math)) }

alias -l sorttop {
  ;$1 = nick, $2 = score, $3 = percent
  if ($2 == 0 || $gettok($3,1,47) <= 0) { return }
  var %a = 1, %b, %c, %score = $2, %l, %hits = $3-

  var %adate = $adate
  var %left = $gettok($adate,1,47)
  var %middle = $gettok($adate,2,47)
  var %right = $gettok($adate,3,47)
  var %adate = $+(%left,/,%middle,/,$mid(%right,3))

  write $qt($scriptdirtop.txt) %score $mid($1,1,10) %adate %hits
  write -c $qt($scriptdirbefore.txt)
  filter -ca $qt($scriptdirtop.txt) $qt($scriptdirbefore.txt) entfilter
  .remove $qt($scriptdirtop.txt)
  .rename $qt($scriptdirbefore.txt) $qt($scriptdirtop.txt)
}

alias f1 {
  if (%space_start) { return }
  %space_top = $iif(%space_top == on,off,on)

  .splay -w $qt($scriptdirmenu.wav) 

  if (%space_top == on) {


    var %a = $gettok(%_display,1,32), %b = $gettok(%_display,2,32)

    drawrect -fms @IRC.buf 01 22 0 0 %a %b

    if (%_display == 800 600 || %_display == 1024 768) { drawcopy -ms @IRC.buf2 0 0 %_display @IRC.buf 0 0 %a %b }
    else { drawcopy -ms @IRC.buf2 0 0 %_display @IRC.buf 0 0 $calc(%a + 12) %b }

    drawtext -o @IRC.buf 00 "Unispace" 55 185 120 Top 10 Scores

    drawtext -o @IRC.buf 00 "Cousine" 20 122 200 $chr(35)
    drawtext -o @IRC.buf 00 "Cousine" 20 164 200 Player
    drawtext -o @IRC.buf 00 "Cousine" 20 308 200 Score
    drawtext -o @IRC.buf 00 "Cousine" 20 417 200 Date
    drawtext -o @IRC.buf 00 "Cousine" 20 539 200 Accuracy

    drawrect -f @IRC.buf 0 2 110 228 585 2

    .timerspace* off
    .timerpause -om 0 20 noop

    var %a = 1, %b, %top = 215, %found = 0, %new, %last, %gotit = 0, %date, %percent
    while ($read($qt($scriptdirtop.txt),%a)) {
      %b = $v1
      %date = $gettok(%b,3,32)
      %new = $gettok(%b,2,32) $gettok(%b,1,32) $gettok(%b,3-4,32)
      %last = $mid($gettok(%space_last,1,32),1,10) $remove($gettok(%b,1,32),$chr(44)) $gettok(%space_last,3-4,32)

      %percent = $gettok(%b,4-,32)

      if (%space_shots == $null) { %space_shots = 0 }
      if (%space_hits == $null) { %space_hits = 0 }

      if (%a <= 10) { 
        inc %top 23
        inc %found
        if (%new == %last) { inc %gotit }

        var %l = $gettok(%new,1,32), %r
        if ($len(%l) > 10) { %l = $mid(%l,1,10) }
        %r = $+(%l,$str($chr(160),$calc(10 - $len(%l))))


        var %l = $bytes($gettok(%new,2,32),b), %ra
        if ($len(%l) > 7) { %l = $mid(%l,1,7) }
        %ra = $+(%l,$str($chr(160),$calc(7 - $len(%l))))        


        drawtext -o @IRC.buf $iif(%new == %last && %gotit == 1,11,0) "Cousine" 17 $iif(%a == 10,110,121) %top %found $makespace %r $makespace %ra $makespace %date $makespace %percent
      }
      if (%a == 10) { break }
      inc %a
    }
    if (%found == 0) { drawtext @IRC.buf 0 "Cousine" 22 325 250 No scores yet! }
    dodrawcopy
  }
  else { 
    if (!%space_over) { update }
    else { drawpic -ms @IRC 0 0 $qt($scriptdirsave.png) }
  } 
}

alias -l makespace { return $str($chr(160),1) }

alias -l pause {
  if (%space_inter) { return }

  .splay -w $qt($scriptdirmenu.wav) 

  .timerspace* off
  .timerpause -om 0 20 noop
  drawtext -o @IRC.buf 00 "Unispace" 55 315 240 Paused
  dodrawcopy
}

alias -l unpause {
  if (%space_inter) { return }

  .splay -w $qt($scriptdirmenu.wav) 

  unset %space_wait

  .timerspace -om 0 20 update
  var %a = 0, %b, %p, %n
  while ($readini($qt($+($scriptdir,%space_level,.ini)),n,$+(wave,%space_wave),$+(n,%a))) {
    %n = $v1
    %b = $gettok(%n,1,32)
    %p = $gettok($hget(space,%b),12,32)
    if (%p) { $+(.timerspaceaddshot,%b) -om 0 %p addshot %b }
    inc %a
  }
  .timerpause off
}

alias -l hud {
  var %l = %space_health, %p
  if (%space_health > 0) {
    if ($len(%l) == 1) { %p = $+(00,%l) }
    elseif ($len(%l) == 2) { %p = $+(0,%l) }
    else { %p = %l }
  }
  else { %p = 000 }
  var %l = %space_points, %w
  if (%space_points > 0) {
    if ($len(%l) == 1) { %w = $+(000000,%l) }
    elseif ($len(%l) == 2) { %w = $+(00000,%l) }
    elseif ($len(%l) == 3) { %w = $+(0000,%l) }
    elseif ($len(%l) == 4) { %w = $+(000,%l) }
    elseif ($len(%l) == 5) { %w = $+(00,%l) }
    elseif ($len(%l) == 6) { %w = $+(0,%l) }
    else { %w = %l }
  }
  else { %w = 0000000 }
  var %l = %space_lives , %q
  if (%space_lives > 0) {
    if ($len(%l) == 1) { %q = $+(0,%l) }
    elseif ($len(%l) == 2) { %q = %l }
  }
  else { %q = 00 }

  var %hits, %shots
  if (%space_shots == $null) { %shots = 000 }
  elseif ($len(%space_shots) == 2) { %shots = $+(0,%space_shots) }
  elseif ($len(%space_shots) == 1) { %shots = $+(00,%space_shots) }
  else { %shots = %space_shots }

  if (%space_hits == $null) { %hits = 000 }
  elseif ($len(%space_hits) == 2) { %hits = $+(0,%space_hits) }
  elseif ($len(%space_hits) == 1) { %hits = $+(00,%space_hits) }
  else { %hits = %space_hits }

  var %_icons, %_text, %_rect

  if (%_display == 800 600) { var %_rect = 530 | var %_icons = 532 | var %_text = 536 }
  elseif (%_display == 1024 768) { var %_rect = 530 | var %_icons = 533 | var %_text = 537 }
  else { var %_rect = 527 | var %_icons = 532 | var %_text = 537 }

  drawrect -f @IRC.buf %space_hud 32 0 %_rect $window(@IRC.buf).w 700

  drawpic -tc @IRC.buf $rgb(127,89,125) 190 %_icons $qt($scriptdirhealth.png)
  drawpic -tc @IRC.buf $rgb(127,89,125) 262 %_icons $qt($scriptdirlives.png)
  drawpic -tc @IRC.buf $rgb(127,89,125) 319 %_icons $qt($scriptdirpoints.png)
  drawpic -tc @IRC.buf $rgb(127,89,125) 446 %_icons $qt($scriptdirpercent.png)

  ;Health
  drawtext @IRC.buf 0 "Cousine" 22 223 %_text $iif($hget(space,player),%p,000)
  ;Lives
  drawtext @IRC.buf 0 "Cousine" 22 290 %_text $iif(%q,$v1,00)
  ;Points
  drawtext @IRC.buf 0 "Cousine" 22 351 %_text $iif(%w,$v1,0000000)
  ;Accuracy
  drawtext @IRC.buf 0 "Cousine" 22 479 %_text %hits $+ / $+ %shots
  drawtext @IRC.buf 0 "Cousine" 22 573 %_text ( $+ $int($round($calc(%space_hits / %space_shots * 100),1)) $+ % $+ )
}

alias -l loadini {
  if ($hget(space,0).item == 1) { 
    inc %space_wave

    if (!$ini($qt($+($scriptdir,%space_level,.ini)),$+(wave,%space_wave))) {
      var %cur = $remove(%space_level,level)
      inc %cur
      %space_level = $+(level,%cur)
      %space_wave = 1
      if (!$isfile($qt($+($scriptdir,%space_level,.ini)))) { 
        %space_win = yes
        unset %space_level

        ;Tally Points
        var %t = %space_points
        if (%space_shots == $null) { var %space_shots = 000 }
        if (%space_hits == $null) { var %space_hits = 000 }
        if ($len(%space_hits) == 1) { %space_hits = $+(00,%space_hits) }
        elseif ($len(%space_hits) == 2) { %space_hits = $+(0,%space_hits) }
        if ($len(%space_shots) == 1) { %space_shots = $+(00,%space_shots) }
        elseif ($len(%space_shots) == 2) { %space_shots = $+(0,%space_shots) }
        var %h = %space_hits $+ / $+ %space_shots ( $+ $int($round($calc(%space_hits / %space_shots * 100),1)) $+ % $+ )
        unset %space_over
        %space_health = 0
        var %adate = $adate
        var %left = $gettok($adate,1,47)
        var %middle = $gettok($adate,2,47)
        var %right = $gettok($adate,3,47)
        var %adate = $+(%left,/,%middle,/,$mid(%right,3))
        %space_last = $mnick $bytes(%space_points,b) %adate %h
        sorttop $mnick %t %h

        halt
      }
      else {
        set -u3 %space_start $ctime
        set $+(-u,$readini($qt($+($scriptdir,%space_level,.ini)),settings,protect)) %space_protect yes
        loadsettings $qt($+($scriptdir,%space_level,.ini))
        inc %space_points %space_clear
        hadd -m space player 393 487 %space_image %space_health %space_w %space_h up %space_damage %space_speed player
      }
    }
    else { loadsettings $qt($+($scriptdir,%space_level,.ini)) }
    drawpic -c @IRC.buf2 0 0 %space_background
    .timerspaceaddshot* off
    var %a = 0, %b, %act, %shot
    hdel -w space missles*
    while ($readini($qt($+($scriptdir,%space_level,.ini)),$+(wave,%space_wave),$+(n,%a))) {
      %b = $v1
      %act = $gettok(%b,1,32)
      hadd -m space %b

      %shot = $gettok($hget(space,%act),12,32)
      $+(.timerspaceaddshot,%act) -om 0 %shot addshot %act
      inc %a
    }   
  }
}

alias -l loadsettings {
  var %a = $qt($+($scriptdir,$nopath($1)))
  if (!$isfile(%a)) { return }
  %space_speed = $readini(%a,settings,speed)

  ;%space_shot = $readini(%a,settings,shot)

  if (%_display == 800 600) { %space_shot = 400 }
  elseif (%_display == 1024 768) { %space_shot = 600 }
  else { %space_shot = 800 }

  %space_remainder = $readini(%a,settings,remainder)
  %space_w = $readini(%a,settings,width)
  %space_h = $readini(%a,settings,height)
  %space_damage = $readini(%a,settings,damage)
  %space_pershot = $readini(%a,settings,pershot)
  %space_misslespeed = $readini(%a,settings,misslespeed)

  %space_background = $qt($+($scriptdir,$readini(%a,settings,background)))

  %space_hud = $readini(%a,settings,hud)
  %space_clear = $readini(%a,settings,clear)
}

alias space {
  if ($hget(space)) { hfree space }
  hmake space 100
  unset %space_* %new_* %_dead %_damage %_owner %_owner2

  if ($isfile($qt($scriptdirsave.png))) { .remove $qt($scriptdirsave.png) }

  if (!%_display) { %_display = 800 600 }

  .timerspace* off
  hdel -w space player_new*
  hdel -w space missles*

  if ($window(@IRC) && $window(@IRC.buf)) { window -c @IRC | window -c @IRC.buf | window -c @IRC.buf2 } 

  var %_w = $gettok(%_display,1,32), %_h = $gettok(%_display,2,32)

  if (%_display == 800 600) { window -pdCw0k0ard +befl @IRC -1 -1 $calc(%_w - 12) %_h }
  elseif (%_display == 1024 768) { window -pdCw0k0ard +befl @IRC -1 -1 $calc(%_w - 12) $calc(%_h - 7) }
  else { window -pdCw0k0arB +fd @IRC -1 -1 %_w %_h }

  window -pdCw0k0h +befl @IRC.buf -1 -1 800 600
  window -pdCw0k0h +befl @IRC.buf2 -1 -1 %_display

  drawrect -fms @IRC 01 22 0 0 %_display

  %space_points = 0
  %space_wave = 1
  %space_level = level1
  %space_owned = player
  %space_image = player.png
  %space_health = $readini($qt($scriptdirlevel1.ini),settings,health)
  %space_lives = $readini($qt($scriptdirlevel1.ini),settings,lives)
  %space_big = ball.png square.png
  %space_shots = 0
  %space_hits = 0
  %space_background = $readini($qt($scriptdirlevel1.ini),settings,background)

  loadsettings $qt($scriptdirlevel1.ini)

  set $+(-u,$readini($qt($+($scriptdir,%space_level,.ini)),settings,protect)) %space_protect yes

  set -u2 %space_start $ctime

  hadd -m space player 393 487 %space_image %space_health %space_w %space_h up %space_damage %space_speed player 0 1000

  .timerspace -om 0 20 update
  .timerspaceaddshot* off

  var %a = 0, %b, %act, %shot
  hdel -w space missles*
  while ($readini($qt($+($scriptdir,%space_level,.ini)),$+(wave,%space_wave),$+(n,%a))) {
    %b = $v1
    %act = $gettok(%b,1,32)
    hadd -m space %b

    %shot = $gettok($hget(space,%act),12,32)
    $+(.timerspaceaddshot,%act) -om 0 %shot addshot %act
    inc %a
  }
  .timerpause off
  titlebar @IRC Invaders $sversion - by $sauthor $syear

  drawpic -c @IRC.buf2 0 0 %space_background
  .splay -p $qt($scriptdirintro.mp3)
  .splay -qp $qt($scriptdirbg.mp3)
}

alias -l draw {
  drawrect -f @IRC.buf2 01 22 0 0 800 600
  write -c $qt($scriptdirbg.txt)
  var %a = 1, %b, %c
  while (%a <= 1200) {
    drawrect -fe @IRC.buf2 $r(88,97) $r(1,3) $r(0,$window(@IRC).w) $r(0,$window(@IRC).h) 2 2
    inc %a
  }
}

alias -l explode {
  ;$1=x,$2=y,$3=w,$4=h
  ; The real root cause of the repeating "/drawpic: couldn't load 20" error (found via a live debug dump of this
  ; alias's own raw arguments: "raw=[%-x %-y %-w %-h]", word for word) wasn't in this script at all -- it was an
  ; engine bug in the %variable substitution scanner, which only recognized a "%" as starting a variable reference
  ; when the very next character was alphanumeric or '_', even though it already allowed '-' WITHIN a name once
  ; one had started. That silently excluded any name that's entirely punctuation right after the '%', like %-x,
  ; %-y, %-w, %-h and %-p -- exactly the names /endplayer uses for the dying player's own saved coordinates before
  ; calling "explode %-x %-y %-w %-h". Those four never got substituted at all: explode() received the four
  ; literal, unexpanded words "%-x" "%-y" "%-w" "%-h" as $1-$4, not real numbers, so "$1 - 20" below saw a
  ; non-numeric operand and left %new_x holding multi-word garbage text instead of a number -- which is what went
  ; on to shift every argument after it in the /drawpic call, landing one of the real width/height numbers in the
  ; filename slot. Fixed in the engine itself (the variable-name scanner), not here -- this script was always
  ; correct mIRC. The guard below is kept anyway as ordinary defense-in-depth (a dying entity's hash record being
  ; read at the exact moment it's torn down is still a real, if rare, possibility), guarding on actual emptiness
  ; ($x == $null) rather than mIRC's bare falsy test since x/y are real screen coordinates and 0 is a legitimate
  ; one -- "!$1" would wrongly skip an explosion drawn right at the playfield's left/top edge.
  if ($1 == $null || $2 == $null || $3 == $null || !$4) { return }
  var %new_x = $1 - 20
  var %new_y = $2 - 20
  var %new_w = $3 + 40
  var %new_h = $4 + 40
  if (%new_x) { drawpic -tmsc @IRC.buf $rgb(127,89,125) %new_x %new_y %new_w %new_h $qt($scriptdirexplode.png) }
}

alias -l update {

  var %a = $gettok(%_display,1,32), %b = $gettok(%_display,2,32)

  if (%_display == $window(-1).w $window(-1).h) { drawcopy -ms @IRC.buf2 0 0 %_display @IRC.buf 0 0 $calc(%a + 12) %b }
  else { drawcopy -ms @IRC.buf2 0 0 %_display @IRC.buf 0 0 %a %b }

  if (!$hget(space,player)) { return }

  var %x = $gettok($hget(space,player),1,32), %y = $gettok($hget(space,player),2,32), %rest = $gettok($hget(space,player),3-,32)

  if ($istok(%space_angle,left,32)) { dec %x %space_speed }
  if ($istok(%space_angle,right,32)) { inc %x %space_speed }
  if ($istok(%space_angle,up,32)) { dec %y %space_speed }
  if ($istok(%space_angle,down,32)) { inc %y %space_speed }


  if (%_display != 800 600 && %_display != 1024 768) { var %_xy = 475 - 7 }
  else { var %_xy = 475 }

  if (%x < 11) { %x = 11 }
  if (%x > 735) { %x = 735 }
  if (%y < 8) { %y = 8 }
  if (%y > %_xy) { %y = %_xy }

  hadd -m space player %x %y %rest

  loadini 

  ;Projectiles
  projectiles

  ;Animate
  animate

  ;Draw Items
  var %a = 1, %o, %player1, %player2, %q, %__xx, %__yy, %__zimage, %__ww, %__hh, %health
  while ($hget(space,%a).item) {
    %o = $v1
    %q = $hget(space,%o)

    %__xx = $gettok(%q,1,32)
    %__yy = $gettok(%q,2,32)
    %__zimage = $gettok(%q,3,32)
    %health = $gettok(%q,4,32)
    %__ww = $gettok(%q,5,32)
    %__hh = $gettok(%q,6,32)

    %player1 = %o

    if ($istok(%_dead,%player1,32)) { explode %__xx %__yy %__ww %__hh }

    elseif ($istok(%_damage,%player1,32)) {
      var %rest = $remove($gettok(%__zimage,1,46),2)
      if (%player1 == player && %space_protect) { drawpic -tc @IRC.buf $rgb(127,89,125) %__xx %__yy $qt($scriptdirplayer3.png) }
      else { drawpic -tc @IRC.buf $rgb(127,89,125) %__xx %__yy $qt($scriptdir $+ %rest $+ $iif($right(%rest,1) == 3,.png,2.png)) }
    }
    else {
      if (%space_protect && %player1 == player) { var %img = player3.png }
      else { var %img = %__zimage }
      if (%__xx) { drawpic -tc @IRC.buf $rgb(127,89,125) %__xx %__yy $qt($scriptdir $+ %img) }
    }

    var %z = 1, %t, %j, %xxx, %yyy, %www, %hhh, %imagex

    while ($hget(space,%z).item) {

      %t = $v1
      %j = $hget(space,%t)
      %xxx = $gettok(%j,1,32)
      %yyy = $gettok(%j,2,32)
      %www = $gettok(%j,5,32)
      %hhh = $gettok(%j,6,32)
      %imagex = $gettok(%j,3,32)

      %player2 = %t
      if (%player1 != %player2) {

        var %p1.x = %__xx , %p1.y = %__yy , %p2.x = %xxx , %p2.y = %yyy , %p1.w = %__ww , %p1.h = %__hh, %p2.w = %www, %p2.h = %hhh
        var %g = $onpoly( 4,4, [ $replace(%p1.x %p1.y $calc(%p1.x + %p1.w) %p1.y $calc(%p1.x + %p1.w) $calc(%p1.y + %p1.h) %p1.x $calc(%p1.y + %p1.h),$chr(32),$chr(44)) ] , [ $replace(%p2.x %p2.y $calc(%p2.x + %p2.w) %p2.y $calc(%p2.x + %p2.w) $calc(%p2.y + %p2.h) %p2.x $calc(%p2.y + %p2.h),$chr(32),$chr(44)) ] )
        if (%g) {

          if ($istok(%_dead,%player2,32)) { explode %xxx %yyy %www %hhh }

          set %_damage $remove(%_damage,%player1,%player2) %player1 %player2

          if (missles* !iswm %player1 && missles* !iswm %player2) {
            if (new_player* iswm %player1 && player == %player2) {
              .timerspaceundamage -om 1 200 unset %_damage


              if (!%space_protect) {
                endplayer %player2 %player1
                set -u2 %_dead $remove(%_dead,%player1,%player2) %player1 %player2
                $+(.timerspaceaddshot,%player1) off
                $+(.timerspacedelete,%player1) -om 1 200 hdel -m space %player1
              }


              if (%player1 != player && %player2 != player) { %a = 0 }
              break
            }
            elseif (new_player* iswm %player2 && player == %player1) {
              .timerspaceundamage -om 1 200 unset %_damage


              if (!%space_protect) {
                endplayer %player1 %player2
                set -u2 %_dead $remove(%_dead,%player1,%player2) %player1 %player2
                $+(.timerspaceaddshot,%player2) off
                $+(.timerspacedelete,%player2) -om 1 200 hdel -m space %player2
              }


              if (%player1 != player && %player2 != player) { %a = 0 }
              break
            }
          }
          elseif (missles* iswm %player1 && missles* iswm %player2) {
            var %owner1 = $hget(space,%player1),10,32), %owner2 = $hget(space,%player2),10,32)
            if (%owner1 == player || %owner2 == player) { inc %space_hits }
            if (missles* iswm $1) { hdel -m space $1 }
            if (missles* iswm $2) { hdel -m space $2 }
          }
          .timerspaceundamage -om 1 200 unset %_damage
          deducthealth %player1 %player2
          if (%player1 != player && %player2 != player) { %a = 0 }
          break
        }
      }
      inc %z
    }
    inc %a
  }

  hud

  if (%space_win) {
    .timerspace* off
    .timerpause off
    drawcopy -ms @IRC.buf2 0 0 %_display @IRC.buf 0 0 %_display
    drawtext -o @IRC.buf 00 "Unispace" 55 270 60 You Win!
    drawtext -o @IRC.buf 00 "Cousine" 16 190 140 Well you fought your way through hell to
    drawtext -o @IRC.buf 00 "Cousine" 16 190 160 get here, now you beat the game! entropy
    drawtext -o @IRC.buf 00 "Cousine" 16 190 180 salutes you, $+($mnick,!)
    drawpic -tc @IRC.buf $rgb(127,89,125) 285 220 $qt($scriptdirtrophy.png)
    dodrawcopy
    return
  }
  elseif ($timer(pause) && !%space_over) { 
    drawtext -o @IRC.buf 00 "Unispace" 55 315 240 Paused
  }  
  elseif (%space_start) { 
    drawtext -o @IRC.buf 00 "Unispace" 55 310 240 level
    drawtext -o @IRC.buf %space_hud "Unispace" 55 490 240 $mid(%space_level,6,32)
    drawtext @IRC.buf 00 "Cousine" 25 356 300 Ready? Go!
    unset %_dead
  }
  ;Copy
  dodrawcopy
}

alias -l deducthealth {
  var %a = $hget(space,player)

  ;If player, and protection, return...
  if ($1 == player && %space_protect) || ($2 == player && %space_protect) { return }

  ;If both are enemies...
  if (new_player* iswm $1 && new_player* iswm $2) { return }

  ;If enemy is $1, and missle is $2, and enemy is owner of missle...
  ;if (new_player* iswm $1 && missles* iswm $2 && new_player* iswm $gettok($hget(space,$2),10,32)) { return }

  ;If enemy is $2, and missle is $1, and enemy is owner of missle...
  ;if (new_player* iswm $2 && missles* iswm $1 && new_player* iswm $gettok($hget(space,$1),10,32)) { return }

  ;If $1 is a missle, and owner is not player...
  if (missles* iswm $1 && $gettok($hget(space,$1),10,32) != player) { %_owner = $gettok($hget(space,$gettok($hget(space,$1),10,32)),8,32) }

  ;If $2 is a missle, and owner is not player...
  if (missles* iswm $2 && $gettok($hget(space,$2),10,32) != player) { %_owner2 = $gettok($hget(space,$gettok($hget(space,$2),10,32)),8,32) }

  ;If $1 is missle, and no owner?
  var %owned = $gettok($hget(space,$1),10,32)
  if (missles* iswm $1 && !$hget(space,%owned)) { hdel -m space $1 | return }

  ;If $2 is missle, and no owner?
  var %owned = $gettok($hget(space,$2),10,32)
  if (missles* iswm $2 && !$hget(space,%owned)) { hdel -m space $2 | return }

  object $1
  object $2
}

alias -l dodrawcopy {
  if (%_display == 800 600) { var %di_x = $gettok(%_display,1,32) | var %di_y = $gettok(%_display,2,32) }
  elseif (%_display == 1024 768) { var %di_x = $calc($gettok(%_display,1,32) + 2) | var %di_y = $calc($gettok(%_display,2,32) + 2) }
  else { var %di_x = $calc($gettok(%_display,1,32) + 18) | var %di_y = $calc($gettok(%_display,2,32) + 16) }
  drawcopy -ms @IRC.buf 0 0 800 600 @IRC 0 0 %di_x %di_y
}

alias -l object {
  if (!$1) { return }
  var %p = 1, %j, %r, %_x, %_y, %_w, %_h, %_health, %a, %_dmg, %_angle, %_owned, %_speed, %_move, %_shoot, %_points
  var %q = $1, %b = $2

  tokenize 32 $hget(space,%q)

  var %_x = $1
  var %_y = $2
  var %_i = $3
  var %_health = $4
  var %_w = $5
  var %_h = $6
  var %_angle = $7
  var %_initdmg = $8
  var %_speed = $9
  var %_owned = $10
  var %_move = $11
  var %_shoot = $12
  var %_points = $13


  if (new_player* iswm %q) { var %_dmg = %space_damage }
  elseif (%q == player && %_owner) { var %_dmg = %_owner | unset %_owner }
  elseif (%q == player && %_owner2) { var %_dmg = %_owner2 | unset %_owner2 }
  else { var %_dmg = $gettok($hget(space,%_owned),8,32) }

  if (%q == player) { dec %space_health %_dmg }

  dec %_health %_dmg

  if (%_health <= 0) {

    set -u2 %_dead $remove(%_dead,%q) %q

    if (player == %q) { endplayer %q }
    elseif (player == %_owned && missles* iswm %q) { 
      inc %space_points %space_pershot
      inc %space_hits
      checkhdel %q
    }
    elseif (new_player* iswm %q) {
      inc %space_points $13
      ; BUGFIX: "explode.mid" was never actually a MIDI file -- checked the real bytes on disk and it's a plain
      ; WAV/PCM recording (RIFF/WAVE, 16-bit mono 22050Hz) that just happened to be saved with a ".mid" extension.
      ; "-m" forces this engine's (and real mIRC's own) MIDI sequencer device, which correctly refuses to play
      ; PCM audio data -- "MCI error 296: ... may be corrupt, not in the correct format" was a completely
      ; accurate description of a real WAV file being opened as if it were MIDI, not a bug in how /splay or its
      ; underlying MCI calls work. Switched to "-w" (wave) and the matching ".wav" extension, matching every
      ; OTHER sound this script plays via /splay -w (menu.wav, bg.mp3/intro.mp3 use -p instead, for mp3). The
      ; actual file on disk also needs to be renamed from explode.mid to explode.wav to match -- its contents
      ; don't need to change at all, since they were already a valid WAV the whole time.
      .splay -w $qt($scriptdirexplode.wav)
      $+(.timercheckhdel,%q) -om 1 100 checkhdel %q
      $+(.timerspaceaddshot,%q) off
    }
    else { checkhdel %q }
  }
  else {
    if (player == %_owned && missles* iswm %q) { inc %space_hits }
    hadd -m space %q %_x %_y %_i %_health %_w %_h %_angle %_initdmg %_speed %_owned %_move %_shoot %_points
  }
}

alias -l checkhdel { if ($hget(space)) { hdel -m space $1- } }



alias -l endplayer {
  var %q = $1, %r = $2
  if (!$1) { return }
  if (%q == player) {

    dec %space_lives 1

    if (%space_lives <= 0) {

      set -u2 %space_inter yes

      .timerspace off

      var %-p = $hget(space,player)
      var %-x = $gettok(%-p,1,32)
      var %-y = $gettok(%-p,2,32)
      var %-w = $gettok(%-p,5,32)
      var %-h = $gettok(%-p,6,32)

      explode %-x %-y %-w %-h


      ;Tally Points
      var %t = %space_points
      if (%space_shots == $null) { var %space_shots = 000 }
      if (%space_hits == $null) { var %space_hits = 000 }
      if ($len(%space_hits) == 1) { %space_hits = $+(00,%space_hits) }
      elseif ($len(%space_hits) == 2) { %space_hits = $+(0,%space_hits) }
      if ($len(%space_shots) == 1) { %space_shots = $+(00,%space_shots) }
      elseif ($len(%space_shots) == 2) { %space_shots = $+(0,%space_shots) }
      var %h = %space_hits $+ / $+ %space_shots ( $+ $int($round($calc(%space_hits / %space_shots * 100),1)) $+ % $+ )
      %space_points = %t
      %space_over = yes
      %space_health = 0
      var %adate = $adate
      var %left = $gettok($adate,1,47)
      var %middle = $gettok($adate,2,47)
      var %right = $gettok($adate,3,47)
      var %adate = $+(%left,/,%middle,/,$mid(%right,3))
      %space_last = $mnick $bytes(%space_points,b) %adate %h
      .timersorttop -om 1 50 sorttop $mnick %t %h


      drawtext -o @IRC.buf 00 "Unispace" 55 260 240 Game Over
      .timerdrawsave -om 1 100 drawsave -b32 @IRC $qt($scriptdirsave.png)

      unset %new_*

      .splay stop
      .splay -w $qt($scriptdirexplode.wav)

      .timerspaceaddshot* off
      .timerspacetopscores -o 1 2 f1
    }
    else {

      .splay -w $qt($scriptdirexplode.wav)

      set -u2 %space_inter yes

      %space_health = %space_remainder
      var %-p = $hget(space,player)
      var %-x = $gettok(%-p,1,32)
      var %-y = $gettok(%-p,2,32)
      var %-w = $gettok(%-p,5,32)
      var %-h = $gettok(%-p,6,32)

      explode %-x %-y %-w %-h

      if ($2) {
        var %-p = $hget(space,$2)
        hdel -m space $2
        var %-x = $gettok(%-p,1,32)
        var %-y = $gettok(%-p,2,32)
        var %-w = $gettok(%-p,5,32)
        var %-h = $gettok(%-p,6,32)

        if (%-p) { explode %-x %-y %-w %-h }
      }

      hdel -m space player

      .timerspaceundeadx -o 1 1 hadd -m space player 393 487 %space_image %space_health %space_w %space_h up %space_damage %space_speed player 0 100

      set -u2 %space_start $ctime
      unset %space_angle
      set $+(-u,$readini($qt($+($scriptdir,%space_level,.ini)),settings,protect)) %space_protect yes
    }
  }
}

alias -l stats {
  var %a = $hget(space,$1)
  if (!%a) { return }
  tokenize 32 %a
  if ($prop == x) { return $1 }
  elseif ($prop == y) { return $2 }
  elseif ($prop == image) { return $3 }
  elseif ($prop == health) { return $4 }
  elseif ($prop == w) { return $5 }
  elseif ($prop == h) { return $6 }
  elseif ($prop == angle) { return $7 }
  elseif ($prop == damage) { return $8 }
  elseif ($prop == speed) { return $9 }
  elseif ($prop == owner) { return $10 }
  elseif ($prop == movement) { return $11 }
  elseif ($prop == shot) { return $12 }
  elseif ($prop == points) { return $13 }
}


alias -l projectiles {

  var %_a = 1, %_b, %_c, %_d, %_health, %_speed, %_owned, %_points, %_desc, %_move, %_shoot, %_points, %found
  while ($hfind(space,missles*,%_a,w).item) {
    %_b = $v1
    %_c = $hget(space,%_b)

    tokenize 32 %_c

    %found = no

    var %_x = $1
    var %_y = $2
    var %_i = $3
    var %_health = $4
    var %_w = $5
    var %_h = $6
    var %_ang = $7
    var %_dmg = $8
    var %_speed = $9
    var %_owned = $10
    var %_move = $11
    var %_shoot = $12
    var %_points = $13

    if (%_ang == up) { dec %_y %space_misslespeed }
    if (%_ang == down) { inc %_y %space_misslespeed }

    if (%_y < 0 || %_y > 600) { %found = yes }
    if (%_x < 0 || %_x > 800) { %found = yes }

    if (%found == yes) { hdel -m space %_b | %_a = 0 }

    else {
      if ($istok(%space_big,%_i,32)) { var %img = missles3.png | %_w = 50 | %_h = 31 | dec %_x 20 | inc %_y 30 }
      else { var %img = $iif(%_ang == down,missles2.png,missles.png) }
      hadd -m space %_b %_x %_y %_i 10 %_w %_h %_ang %_dmg %_speed %_owned %_move %_shoot %_points
    }
    inc %_a
  }
}

alias -l addshot {
  ;$1 = shooting entity

  if ($hfind(space,missles*,0,w).item >= 3) { return }  

  var %n = $1

  tokenize 32 $hget(space,$1)
  var %x = $1
  var %y = $2
  var %i = $3
  var %health = $4
  var %w = $5
  var %h = $6
  var %a = $7
  var %dmg = $8

  var %sp = $9

  var %move = $11
  var %shoot = $12
  var %points = $13
  var %xx = $calc(%x + (%w / 2 - 10))

  var %yy = $calc(%y + %h)
  var %r = $r(1000,9999)

  if (%yy <= 0 || %yy >= 550) { return }

  .splay -w $qt($scriptdirlaser.wav)
  if ($rand(0,1)) {
    if ($istok(%space_big,%i,32)) { dec %xx 20 | hadd -m space $+(missles,%r) %xx %yy missles3.png 10 48 20 down %dmg 0.8 %n %move %shoot %points }
    else { hadd -m space $+(missles,%r) %xx %yy missles2.png 10 20 10 down %dmg 0.8 %n %move %shoot %points }
  }
}

alias -l animate {
  var %a = 1, %b, %c, %x, %y, %w, %h, %i, %d, %an, %p, %health, %dmg, %sp, %owned, %move, %shoot, %points
  while ($hfind(space,new_player*,%a,w).item) {
    %b = $v1
    %c = $hget(space,%b)

    var %t = %c
    tokenize 32 %t
    %x = $1
    %y = $2
    %i = $3
    %health = $4
    %w = $5
    %h = $6
    %an = $7
    %dmg = $8
    %sp = $9
    %owned = $10
    %move = $11
    %shoot = $12
    %points = $13

    if (%move != 2) { hadd -m space %b %x $iif(%y < 600,$calc(%y + $9),-100) %i %health %w %h %an %dmg %sp %owned %move %shoot %points }
    else { hadd -m space %b %x %y %i %health %w %h %an %dmg %sp %owned %move %shoot %points }
    if (%move == 1) { backnforth %b }
    inc %a
  }
}


alias -l backnforth {
  ;$1 = item
  var %n = $1
  var %a = 1, %b, %c, %d, %x, %y, %w, %h, %i, %d, %an, %p, %health, %sp, %owned, %dmg, %org, %move, %shoot, %points
  %c = $hget(space,%n)
  tokenize 32 %c
  %x = $1
  %y = $2
  %i = $3
  %health = $4
  %w = $5
  %h = $6
  %an = $7
  %dmg = $8
  %sp = $9
  %owned = $10
  %move = $11
  %shoot = $12
  %points = $13

  var %right = $calc(780 - %w)
  if (!$eval($+(%,%n,_x),3)) { set $+(%,%n,_angle) $gettok(left right,$r(1,2),32) }

  ; BUGFIX: "inc %x = 5" isn't valid /inc syntax (the command just takes the amount directly, "inc %x 5" --
  ; no "=") -- the "= 5" leftover after the variable name got passed on as the amount text, which doesn't parse
  ; as a number, so this fired "/inc: the amount must be a number" (and left %x unchanged) on every single call
  ; where a leftward-drifting enemy's x position had actually dipped below 0 and needed to bounce back right.
  elseif ($eval($+(%,%n,_x),3) < 0) { inc %x 5 | set $+(%,%n,_angle) right }
  elseif ($eval($+(%,%n,_x),3) > %right) { dec %x 5 | set $+(%,%n,_angle) left }

  if ($eval($+(%,%n,_angle),3) == left) { dec %x %sp | set $+(%,%n,_x) %x }
  if ($eval($+(%,%n,_angle),3) == right) { inc %x %sp | set $+(%,%n,_x) %x }
  hadd -m space %n %x %y %i %health %w %h %an %dmg %sp %owned %move %shoot %points
}
