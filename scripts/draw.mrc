; Draw! script 1.0b
; by ch1zra
; ch1zra@gmail.com
; #Scripters @ irc.krstarica.com
; http://uptheirons.co.sr/
; Copyright © Up The Irons ! inc. Scripting Team

; ~~~ On Load & Unload ~~~

on 1:load: {
    window -wkp +df @Draw 
    window -a @draw
    clears
}

on 1:unload: {
  if ($window(@Draw)) { 
    window -c @draw
  }
}

; ~~~ Menus ~~~

menu channel,status {
  Draw!:Draw.win
}

menu menubar {
  Unload Draw!:unload -rs $script
}

menu @Draw {
  dclick:clears
  Draw
  .Wire Square:Draw.square
  .Circle:circle
  .Sine:sine
  .Cosine:cosine
  .Tangent:tangent
  .Cotangent:cotangent
  Clear Screen:drawfill @Draw 1 0 1 1
  Save Graphic:save
  About:About
  Close:window -c @Draw
}

menu @DrawAbout {
  dclick:window -c @DrawAbout
}
; ~~~ Drawings ~~~ 

alias Draw.win {
  window -awkp +df @Draw 
  drawfill @Draw 1 1 1 1 
}

alias -l save {
  var %save $Input(Save graphic as *.bmp,e,Saving) 
  if (%save != $null) {
    drawsave @Draw %save $+ .bmp
    echo [Draw!] Graphic saved as $mircdir $+ %save $+ .bmp
  }
  else echo [Draw!] No filename provided, saving canceled...
}

alias -l circle {
  drawfill @Draw 1 0 1 1
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 10 : $left(Drawing circle,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %radius $input(Enter circle radius (in pixels) :,e,Radius)
  if (%radius == $null) {
    drawtext @Draw 4 FixedSys 10 5 30 : Error, no radius entry. Aborting... 
    halt 
  }
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 30 : $left(Radius is set to %radius pixels,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %dot $input(Enter circle thickness (in pixels) :,e,Thickness)
  if (%dot == $null) {
    drawtext @Draw 4 FixedSys 10 5 50 : Error, no dot thickness entry. Aborting... 
    halt 
  }
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 50 : $left(Thickness is set to %dot pixels,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %i 0
  while (%i <= 360) {
    drawdot  @Draw 4 %dot $calc($sin(%i).deg * %radius + 300) $calc($cos(%i).deg * %radius + 300 )
    inc %i 0.5
    var %t 1
    while (%t <= 300) { inc %t }
  }
}

alias Draw.square {
  drawfill @Draw 1 0 1 1
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 10 : $left(Drawing wired square,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %i 0
  set %side $input(Enter size of square side (in pixels) :,e,Side)
  if (%side == $null) {
    drawtext @Draw 4 FixedSys 10 5 30 : Error, no square side entry. Aborting... 
    halt 
  }
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 30 : $left(Square side set to %side pixels,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  set %end $calc(100 + %side)
  set %step $input(Step for wire (in pixels) :,e,Stepping)
  if (%step == $null) || (%step == 0) {
    drawtext @Draw 4 FixedSys 10 5 50 : Error, no wire step entry. Aborting... 
    halt 
  }
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 50 : $left(Wire step set to %step pixels,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %i 0
  while (%i <= %side) {
    drawline @Draw 12 1  $calc(100 + %i) 100  $calc(%i  + 100)  %end
    inc %i %step
    var %t 1
    while (%t <= 500) { inc %t }
  }
  var %i 0
  while (%i  <= %side) {
    drawline @Draw 12 1   100 $calc(100 + %i) %end  $calc(100 + %i)   
    inc %i %step
    var %t 1
    while (%t <= 500) { inc %t }
  }
  unset %side
  unset %step
  unset %end
}

alias -l sine {
  drawfill @Draw 1 0 1 1
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 10 : $left(Drawing sine function,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  set %deg $input(Enter interval in degrees (360 is full),e,Degree)
  if (%deg == $null) {
    drawtext @Draw 4 FixedSys 10 5 30 : Error, no degree entry. Aborting... 
    halt 
  }
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 30 : $left(Interval is %deg $+ ° (degrees),%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %i 0
  drawtext @Draw 12 fixedsys 10 205 85 Amplitude
  drawtext @Draw 12 fixedsys 10 $calc(250 + %deg) 205  Angle
  drawline @Draw 8 1 100 200 $calc(250 + %deg) 200
  drawline @Draw 8 1 200 100 200 300
  while (%i <= %deg) {
    drawline @Draw 4 1  $calc(200 + %i) 200 $calc(200 + %i) $calc($sin(%i).deg * 100 + 200)
    inc %i 4
    var %t 1
    while (%t <= 1000) { inc %t }
  }
}

alias -l cosine {
  drawfill @Draw 1 0 1 1
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 10 : $left(Drawing cosine function,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  set %deg $input(Enter interval in degrees (360 is full),e,Degree)
  if (%deg == $null) {
    drawtext @Draw 4 FixedSys 10 5 30 : Error, no degree entry. Aborting... 
    halt 
  }
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 30 : $left(Interval is %deg $+ ° (degrees),%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %i 0
  drawtext @Draw 12 fixedsys 10 205 85 Amplitude
  drawtext @Draw 12 fixedsys 10 $calc(250 + %deg) 205  Angle
  drawline @Draw 8 1 100 200 $calc(250 + %deg) 200
  drawline @Draw 8 1 200 100 200 300
  while (%i <= %deg) {
    drawline @Draw 4 1  $calc(200 + %i) 200 $calc(200 + %i) $calc($cos(%i).deg * 100 + 200)
    inc %i 4
    var %t 1
    while (%t <= 1000) { inc %t }
  }
}


alias -l tangent {
  drawfill @Draw 1 0 1 1
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 10 : $left(Drawing tangent function,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  set %deg $input(Enter interval in degrees (360 is full),e,Degree)
  if (%deg == $null) {
    drawtext @Draw 4 FixedSys 10 5 30 : Error, no degree entry. Aborting... 
    halt 
  }
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 30 : $left(Interval is %deg $+ ° (degrees),%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %i 0
  drawtext @Draw 12 fixedsys 10 205 85 Amplitude
  drawtext @Draw 12 fixedsys 10 $calc(250 + %deg) 205  Angle
  drawline @Draw 8 1 100 200 $calc(250 + %deg) 200
  drawline @Draw 8 1 200 100 200 300
  while (%i <= %deg) {
    drawline @Draw 4 1  $calc(200 + %i) 200 $calc(200 + %i) $calc($tan(%i).deg * 10 + 200)
    inc %i 4
    var %t 1
    while (%t <= 1000) { inc %t }
  }
}

alias -l cotangent {
  drawfill @Draw 1 0 1 1
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 10 : $left(Drawing cotangent function,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  set %deg $input(Enter interval in degrees (360 is full),e,Degree)
  if (%deg == $null) {
    drawtext @Draw 4 FixedSys 10 5 30 : Error, no degree entry. Aborting... 
    halt 
  }
  var %i 0
  while (%i <= 30) {
    drawtext @Draw 4 FixedSys 10 5 30 : $left(Interval is %deg $+ ° (degrees),%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %i 0
  drawtext @Draw 12 fixedsys 10 205 85 Amplitude
  drawtext @Draw 12 fixedsys 10 $calc(250 + %deg) 205  Angle
  drawline @Draw 8 1 100 200 $calc(250 + %deg) 200
  drawline @Draw 8 1 200 100 200 300
  while (%i <= %deg) {
    drawline @Draw 4 1  $calc(200 + %i) 200 $calc(200 + %i) $calc((1 / $tan(%i).deg) * 10 + 200)
    inc %i 4
    var %t 1
    while (%t <= 1000) { inc %t }
  }
}


alias -l about {
  window -awkhdoCp +def @DrawAbout 2 2 400 435
  alias Draw.cube {
    drawfill @DrawAbout 1 0 1 1
    var %i 0
    while (%i <= 400) {
      drawline @DrawAbout 12 1  400 %i %i 0
      inc %i 10
      var %t 1
      while (%t <= 500) { inc %t }
    }
    var %i 0
    while (%i  <= 400) {
      drawline @DrawAbout 12 1   $calc(400  - %i) 400  400 %i 
      inc %i 10
      var %t 1
      while (%t <= 500) { inc %t }
    }
    var %i 0
    while (%i <= 400) {
      drawline @DrawAbout 12 1 0 $calc(400  - %i)  $calc(400  - %i) 400 
      inc %i 10
      var %t 1
      while (%t <= 500) { inc %t }
    }
    var %i 0
    while (%i <= 400) {
      drawline @DrawAbout 12 1  0 $calc(400  - %i) %i 0
      inc %i 10
      var %t 1
      while (%t <= 500) { inc %t }
    }
    unset %side
    unset %step
    unset %end
  }
  var %i 0
  while (%i <= 20) {
    drawtext @DrawAbout 4 FixedSys 30 95 180 [ $+ $left(Draw! script],%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %i 0
  while (%i <= 30) {
    drawtext @DrawAbout 4 FixedSys 10 140 210 - $left(by ch1zra -,%i)
    inc %i
    var %t 1
    while (%t <= 1000) { inc %t }
  }
  var %i 0
  while (%i <= 360) {
    drawdot  @DrawAbout 14 3  $calc($sin(%i).deg * 145 + 203) $calc($cos(%i).deg * 88 + 203)
    inc %i 5
    var %t 1
    while (%t <= 500) { inc %t }
  }
  drawtext @DrawAbout 14 FixedSys 10 110 405 [Doubleclick to close]
  drawtext @DrawAbout 4 FixedSys 10 140 230 ch1zra@gmail.com
  drawtext @DrawAbout 4 FixedSys 10 110 250 http://uptheirons.co.sr/
}

alias -l clears {
  drawfill @Draw 1 0 1 1
  drawtext @Draw 4 FixedSys 40 10 10 Draw! script
  drawtext @Draw 4 FixedSys 10 300 35 v 1.0b
  drawtext @Draw 4 FixedSys 10 10 50 by ch1zra
  drawtext @Draw 4 FixedSys 10 10 70 NOTE : while drawing, do not touch mIRC (it doesn't takes much time)
  drawtext @Draw 4 FixedSys 10 10 170 Super useless, unless you don't know how sine/cosine function graphs look.
  drawtext @Draw 4 FixedSys 10 10 190 Yet, I wanted to release this, just to let people know what they can do with 
  drawtext @Draw 4 FixedSys 10 10 210 /draw & /while commands
  drawtext @Draw 4 FixedSys 10 10 230 Hope you like the graphics :)
  drawtext @Draw 4 FixedSys 10 10 300 ch1zra@gmail.com
  drawtext @Draw 4 FixedSys 10 10 320 #Scripters @ irc.krstarica.com 
  drawtext @Draw 4 FixedSys 10 10 340 http://uptheirons.co.sr/
  drawtext @Draw 4 FixedSys 10 10 500 PS ( FixedSys font pwnz! )
}

; Draw! script
; by ch1zra
; ch1zra@gmail.com
; #Scripters @ irc.krstarica.com
; http://uptheirons.co.sr/
; Copyright © Up The Irons ! inc. Scripting Team
