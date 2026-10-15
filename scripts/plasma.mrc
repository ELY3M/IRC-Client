; plasma imitation
; type //plasma
;
alias plasma {
  if ($window(@plasma)) window -c @plasma | if ($window(@plasma.buf)) window -c @plasma.buf
  if ($hget(plasma)) hfree plasma | hmake plasma 100 | window -hp @plasma.buf 0 0 600 600 | window -CdopB +d @plasma 900 0 300 300
  drawrect -nrf @plasma.buf 0 1 0 0 700 700 | var %i 100, %1 $r(0,255), %2 $r(0,255), %w
  while %i { %w = $calc(%i - (100 - %i)) | drawrect -drn @plasma.buf $rgb(%1,$int($calc($sin($calc(%i /7))*255)),%2) 3 $abs($calc(%i - 100)) $abs($calc(%i - 100)) %w %w %w %w | dec %i }
  drawcopy -n @plasma.buf 0 0 100 100 @plasma.buf 110 0 100 100
  drawrect -drnf @plasma.buf 0 1 130 20 60 60 60 60 | drawcopy -n @plasma.buf 0 0 100 100 @plasma.buf 222 0 100 100
  drawcopy -n @plasma.buf 110 0 100 100 @plasma.buf 0 120 100 100 | drawcopy -n @plasma.buf 110 0 100 100 @plasma.buf 330 0 100 100
  drawcopy -n @plasma.buf 0 0 100 100 @plasma.buf 330 120 100 100 | drawcopy -n @plasma.buf 0 0 220 100 @plasma.buf 110 120 220 100
  drawcopy -n @plasma.buf 0 0 450 100 @plasma.buf 0 240 450 100 | drawcopy -n @plasma.buf 0 120 450 100 @plasma.buf 0 360 450 100
  drawcopy -n @plasma.buf 0 0 450 100 @plasma.buf 0 480 450 100 | drawcopy -n @plasma.buf 0 0 100 600 @plasma.buf 440 0 100 600
  hadd plasma z $iif(!$1,5,$1) | hadd plasma t 0 | hadd plasma j 0 | hadd plasma dir 1 | drawdot @plasma.buf
  .timerplasma 0 0 plasma.loop
}

alias -l plasma.loop {
  var %t $hget(plasma,t) | hadd plasma j $calc($hget(plasma,j) + 4) | if ($hget(plasma,j) > 240) hadd plasma j 0
  if (%t > 105) hadd plasma dir 0 | if (%t < -105) hadd plasma dir 1 | if ($hget(plasma,dir)) { inc %t 2 | hadd plasma t %t } | else { dec %t 2 | hadd plasma t %t }
  var %i 0, %x | while %i < 301 { %x = $calc(($abs($sin($calc(%i /100)))* %t)-100) | drawcopy -n @plasma.buf $calc(%x +200) $calc(%i +$hget(plasma,j)) $calc(%x +300) $hget(plasma,z) @plasma 0 %i 300 $hget(plasma,z) | inc %i $hget(plasma,z) }
  drawdot @plasma
}

on *:close:@plasma:{
  plasma.die
}
menu @plasma {
  quality
  .excellent { hadd plasma z 1 }
  .very good { hadd plasma z 2 }
  .good { hadd plasma z 5 }
  .so-so { hadd plasma z 7 }
  .bad { hadd plasma z 10 }
  .disgusting { hadd plasma z 20 }
  quit { plasma.die }
}
alias -l plasma.die { window -c @plasma | window -c @plasma.buf | .timerplasma off | if ($hget(plasma)) hfree plasma }
