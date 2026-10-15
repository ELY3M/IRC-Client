;;;;;;;;;;ASCII Paint;;;;;;;;;;;;;;;;;;;;;;;
alias draw {
  unset %draw.nodraw

  set %draw.curtip NONE
  if (%draw.color1 == $null) { set %draw.color1 $rgb(0,0,0) }
  if (%draw.color2 == $null) { set %draw.color2 $rgb(208,208,208) }
  if (%draw.height == $null) { set %draw.height 10 }
  if (%draw.width == $null) { set %draw.width 30 }
  if (%draw.delay == $null) { set %draw.delay 3000 }
  if (%draw.curcolor == $null) { set %draw.curcolor 1 }
  if (%draw.curcolor2 == $null) { set %draw.curcolor2 0 }
  if (%draw.grid == $null) { set %draw.grid 1 }
  if (%draw.gridx == $null) { set %draw.gridx 11 }
  if (%draw.gridy == $null) { set %draw.gridy 16 }
  window -ac @ASCII Paint
  hadd -m drawcmd 1 hi
  hfree drawcmd
  hadd -m drawtip 1 hi
  hfree drawtip
  window -aBdfhiknop +fnt @ASCII Paint -1 -1 $iif($calc(5 + (%draw.width * %draw.gridx)) > 280, $calc(5 + (%draw.width * %draw.gridx)), 280) $calc(103 + (%draw.height * %draw.gridy))
  window -a @ASCII Paint
  drawfill -nr @ASCII Paint %draw.color2 %draw.color2 1 1
  draw.mainmenu
  draw.grid
  draw.colorbox
  draw.showcolor
  draw.showdim
  draw.tip Welcome to ascii paint.
  drawdot @ASCII Paint
  titlebar @ASCII Paint - Untitled
  unset %draw.send.DESC
}
alias draw.nomenu {
  draw.setcommands 2 21 65 17 echo
  draw.setcommands 70 21 65 17 echo
  draw.setcommands 138 21 65 17 echo
  draw.setcommands 206 21 65 17 echo
  draw.settips 2 21 65 17 NONE
  draw.settips 70 21 65 17 NONE
  draw.settips 138 21 65 17 NONE
  draw.settips 206 21 65 17 NONE
  drawrect -nrf @ASCII Paint %draw.color2 1 2 21 $window(@ASCII Paint).w 17
  draw.mainmenu
  drawdot @ASCII Paint
}
alias draw.mainmenu {
  draw.drawbutton @ASCII Paint 2 2 65 17 File
  draw.setcommands 2 2 65 17 draw.filemenu
  draw.settips 2 2 65 17 File menu
  draw.drawbutton @ASCII Paint 70 2 65 17 Edit
  draw.setcommands 70 2 65 17 draw.editmenu
  draw.settips 70 2 65 17 Edit menu
  draw.drawbutton @ASCII Paint 138 2 65 17 Options
  draw.setcommands 138 2 65 17 draw.gridoptions
  draw.settips 138 2 65 17 Options menu
  draw.drawbutton @ASCII Paint 206 2 65 17 Actions
  draw.setcommands 206 2 65 17 draw.actmenu
  draw.settips 206 2 65 17 Actions menu
  drawdot @ASCII Paint
}
alias draw.filemenu {
  draw.mainmenu
  draw.rdrawbutton @ASCII Paint 2 2 65 17 File
  draw.setcommands 2 2 65 17 draw.nomenu
  drawrect -nrf @ASCII Paint %draw.color2 1 2 21 $window(@ASCII Paint).w 17
  draw.drawbutton @ASCII Paint 2 21 65 17 New
  draw.setcommands 2 21 65 17 draw.new
  draw.drawbutton @ASCII Paint 70 21 65 17 Open
  draw.setcommands 70 21 65 17 draw.open
  draw.drawbutton @ASCII Paint 138 21 65 17 Save
  draw.setcommands 138 21 65 17 draw.save
  draw.drawbutton @ASCII Paint 206 21 65 17 Exit
  draw.setcommands 206 21 65 17 draw.exit
  draw.settips 2 21 65 17 Start from scratch
  draw.settips 70 21 65 17 Open an existing drawing
  draw.settips 138 21 65 17 Save your drawing to load later
  draw.settips 206 21 65 17 Quit the program
  drawdot @ASCII Paint
}
alias -l draw.new { draw.grid | draw.nomenu | titlebar @ASCII Paint - Untitled | unset %draw.send.DESC }
alias draw.editmenu {
  draw.mainmenu
  draw.rdrawbutton @ASCII Paint 70 2 65 17 Edit
  draw.setcommands 70 2 65 17 draw.nomenu
  drawrect -nrf @ASCII Paint %draw.color2 1 2 21 $window(@ASCII Paint).w 17
  draw.drawbutton @ASCII Paint 2 21 65 17 Fill
  draw.setcommands 2 21 65 17 draw.fill
  draw.drawbutton @ASCII Paint 70 21 65 17 Rotate
  draw.setcommands 70 21 65 17 draw.rotate
  draw.drawbutton @ASCII Paint 138 21 65 17 Mirror
  draw.setcommands 138 21 65 17 draw.mirror
  draw.drawbutton @ASCII Paint 206 21 65 17 Flip
  draw.setcommands 206 21 65 17 draw.flip
  drawdot @ASCII Paint
  draw.settips 2 21 65 17 Replace 2nd color with 1st color
  draw.settips 70 21 65 17 Flip the image
  draw.settips 138 21 65 17 Mirror the image left/right
  draw.settips 206 21 65 17 Mirror the image up/down
}
alias draw.actmenu {
  draw.mainmenu
  draw.rdrawbutton @ASCII Paint 206 2 65 17 Actions
  draw.setcommands 206 2 65 17 draw.nomenu
  drawrect -nrf @ASCII Paint %draw.color2 1 2 21 $window(@ASCII Paint).w 17
  ;  draw.drawbutton @ASCII Paint 2 21 65 17 Get
  draw.setcommands 2 21 65 17 echo
  draw.drawbutton @ASCII Paint 70 21 65 17 Send
  draw.setcommands 70 21 65 17 draw.sendto
  draw.drawbutton @ASCII Paint 138 21 65 17 Echo
  draw.setcommands 138 21 65 17 draw.echo
  draw.drawbutton @ASCII Paint 206 21 65 17 Spam
  draw.setcommands 206 21 65 17 draw.getspamchan
  drawdot @ASCII Paint
  draw.settips 2 21 65 17 NONE
  draw.settips 70 21 65 17 Pseudo dcc send images
  draw.settips 138 21 65 17 Show image in your status window
  draw.settips 206 21 65 17 Spam the image to IRC
}
alias draw.optionsmenu {
  draw.mainmenu
  draw.rdrawbutton @ASCII Paint 138 2 65 17 Options
  draw.setcommands 138 2 65 17  draw.nomenu
  drawrect -nrf @ASCII Paint %draw.color2 1 2 21 $window(@ASCII Paint).w 17
  ;draw.drawbutton @ASCII Paint 2 21 65 17 Size
  draw.setcommands 2 21 65 17 echo
  draw.drawbutton @ASCII Paint 70 21 65 17 Grid
  draw.setcommands 70 21 65 17 draw.gridoptions
  draw.drawbutton @ASCII Paint 138 21 65 17 Delay
  draw.setcommands 138 21 65 17 draw.delay
  draw.setcommands 206 21 65 17 echo
  drawdot @ASCII Paint
  draw.settips 2 21 65 17 NONE
  draw.settips 70 21 65 17 Set grid options.
  draw.settips 138 21 65 17 Change the spam delay.
  draw.settips 206 21 65 17 NONE
}
alias -l draw.drawbutton {
  drawrect -nrf $$1 %draw.color2 1 $$2-5
  drawrect -nr $$1 %draw.color1 1 $$2-5
  drawtext -ncr $$1 %draw.color1 Fixedsys 14 $calc(($$2 + ($$4 / 2)) - ($width($$6-,Fixedsys,14) / 2)) $calc(($$3 + ($$5 / 2)) - ($height($$6-,Fixedsys,14) / 2)) $width($$6-,Fixedsys,14) $height($$6-,Fixedsys,14) $$6-
}
alias -l draw.rdrawbutton {
  drawrect -nrf $$1 %draw.color1 1 $$2-5
  drawrect -nr $$1 %draw.color2 1 $$2-5
  drawtext -ncr $$1 %draw.color2 Fixedsys 14 $calc(($$2 + ($$4 / 2)) - ($width($$6-,Fixedsys,14) / 2)) $calc(($$3 + ($$5 / 2)) - ($height($$6-,Fixedsys,14) / 2)) $width($$6-,Fixedsys,14) $height($$6-,Fixedsys,14) $$6-
}
alias -l draw.setcommands {
  hadd -m drawcmd $$1 $+ , $+ $$2 $+ , $+ $$3 $+ , $+ $$4 $$1-
}
alias -l draw.settips {
  hadd -m drawtip $$1 $+ , $+ $$2 $+ , $+ $$3 $+ , $+ $$4 $$1-
}
alias -l draw.grid {
  drawrect -nrf @ASCII Paint %draw.color2 1 2 40 $calc(1 + (%draw.width * %draw.gridx)) $calc(1 + (%draw.height * %draw.gridy))
  hadd -m draw 1 hi
  hfree draw
  if (%draw.grid == 1) {
    drawrect -nr @ASCII Paint %draw.color1 1 2 40 $calc(1 + (%draw.width * %draw.gridx)) $calc(1 + (%draw.height * %draw.gridy))
    set %x 1
    while (%x <= %draw.width) {
      drawline -nr @ASCII Paint %draw.color1 1 $calc(2 + (%x * %draw.gridx)) 40 $calc(2 + (%x * %draw.gridx)) $calc(40 + (%draw.height * %draw.gridy))
      inc %x 1
    }
  }
  set %x 1
  while (%x <= %draw.height) {
    if (%draw.grid == 1) {
      drawline -nr @ASCII Paint %draw.color1 1 2 $calc(40 + (%x * %draw.gridy)) $calc(2 + (%draw.width * %draw.gridx)) $calc(40 + (%x * %draw.gridy))
    }
    hadd -m draw %x $str(0 $+ $chr(44),$calc(%draw.width - 1)) $+ 0
    inc %x 1
  }
  draw.draw

}
alias draw.colorbox {
  drawrect -nrf @ASCII Paint %draw.color2 1 2 $calc(51 + (%draw.height * %draw.gridy)) $window(@ASCII Paint).w 33
  drawrect -nr @ASCII Paint %draw.color1 1 82 $calc(51 + (%draw.height * %draw.gridy)) 89 33
  drawline -nr @ASCII Paint %draw.color1 1 82 $calc(67 + (%draw.height * %draw.gridy)) $calc(82 + (8 * 11)) $calc(67 + (%draw.height * %draw.gridy))
  set %x 1
  while (%x <= 7) {
    drawline -nr @ASCII Paint %draw.color1 1 $calc(82 + (%x * 11)) $calc(51 + (%draw.height * %draw.gridy)) $calc(82 + (%x * 11)) $calc(83 + (%draw.height * %draw.gridy))
    inc %x 1
  }
  set %x 0
  while (%x < 15) {
    drawrect -nf @ASCII Paint %x 1 $calc(83 + (11 * $int($calc(%x / 2)))) $iif($int($calc(%x / 2)) == $calc(%x / 2),$calc(52 + (%draw.height * %draw.gridy)),$calc(68 + (%draw.height * %draw.gridy))) 10 15
    inc %x 1
  }
}
alias draw.showcolor {
  if (%draw.curcolor == $null) { set %draw.curcolor 1 }
  if (%draw.curcolor2 == $null) { set %draw.curcolor2 0 }
  drawrect -nr @ASCII Paint %draw.color1 1 30 $calc(61 + (%draw.height * %draw.gridy)) 25 25
  drawrect -nf @ASCII Paint %draw.curcolor2 1 31 $calc(62 + (%draw.height * %draw.gridy)) 23 23
  drawrect -nr @ASCII Paint %draw.color1 1 20 $calc(51 + (%draw.height * %draw.gridy)) 25 25
  drawrect -nf @ASCII Paint %draw.curcolor 1 21 $calc(52 + (%draw.height * %draw.gridy)) 23 23
}
alias draw.tip {
  drawrect -nfr @ASCII Paint %draw.color2 1 2 $calc(86 + (%draw.height * %draw.gridy)) $calc($window(@ASCII Paint).w - 2) 20
  if ($1) {
    drawtext -nr @ASCII Paint %draw.color1 Fixedsys 14 2 $calc(86 + (%draw.height * %draw.gridy)) $1-
  }
  drawdot @ASCII Paint
}
alias draw.showdim {
  drawrect -nfr @ASCII Paint %draw.color2 1 191 $calc(60 + (%draw.height * %draw.gridy)) $calc($window(@ASCII Paint).w - 191) 10
  drawtext -nr @ASCII Paint %draw.color1 Fixedsys 14 191 $calc(60 + (%draw.height * %draw.gridy)) %draw.width $+ x $+ %draw.height
}
menu @ASCII Paint {
  mouse {
    if (%draw.nodraw) { halt }
    if ($inrect($mouse.x,$mouse.y,2,40,$calc((%draw.width * %draw.gridx)),$calc((%draw.height * %draw.gridy))) == $true) {
      if ($mouse.key & 1) {
        set %draw.col $int($calc((($mouse.x - 2) / %draw.gridx) + 1))
        set %draw.row $int($calc((($mouse.y - 40) / %draw.gridy) + 1))
        if (%draw.row2 != %draw.row) || (%draw.col2 != %draw.col) {
          set %draw.row2 %draw.row
          set %draw.col2 %draw.col
          ;echo -s > %draw.row %draw.col
          drawrect -f @ASCII Paint %draw.curcolor 1 $calc(3 - $iif(%draw.grid == 1,0,0) + ((%draw.col - 1) * %draw.gridx)) $calc(41 - $iif(%draw.grid == 1,0,0) + ((%draw.row - 1) * %draw.gridy)) $calc(%draw.gridx - $iif(%draw.grid == 1,1,0)) $calc(%draw.gridy - $iif(%draw.grid == 1,1,0))

          ; drawrect -f @ASCII Paint %draw.curcolor 1 $calc(3 + ((%draw.col - 1) * %draw.gridx)) $calc(41 + ((%draw.row - 1) * %draw.gridy))  $calc(%draw.gridx - 1) $calc(%draw.gridy - 1)
          ;echo -s hadd -m draw %draw.row $iif(%draw.col > 1,$gettok($hget(draw,%draw.row),1- $+ $calc(%draw.col - 1),44) $+ $chr(44)) $+ %draw.curcolor $+ $iif(%draw.col < %draw.width,$chr(44) $+ $gettok($hget(draw,%draw.row),$calc(%draw.col + 1) $+ -,44))
          hadd -m draw %draw.row $iif(%draw.col > 1,$gettok($hget(draw,%draw.row),1- $+ $calc(%draw.col - 1),44) $+ $chr(44)) $+ %draw.curcolor $+ $iif(%draw.col < %draw.width,$chr(44) $+ $gettok($hget(draw,%draw.row),$calc(%draw.col + 1) $+ -,44))
        }
      }
      if (%draw.curtip != NONE) { draw.tip | set %draw.curtip NONE }
    }
    else {
      set %z 0
      set %x 1
      while (%x <= $hget(drawtip,0).item) {
        set %y $hget(drawtip,$hget(drawtip,%x).item)
        if ($inrect($mouse.x,$mouse.y,$gettok(%y,1,32),$gettok(%y,2,32),$gettok(%y,3,32),$gettok(%y,4,32)) == $true) {
          set %z 1
          if (%draw.curtip != $gettok(%y,5-,32)) {
            set %draw.curtip $gettok(%y,5-,32)
            if (%draw.curtip != NONE) {
              draw.tip %draw.curtip
            }
            else {
              draw.tip
            }
          }
        }
        inc %x 1
      }
      if (%z == 0) && (%draw.curtip != NONE) { draw.tip | set %draw.curtip NONE }
    }
  }
  rclick {
    ;check if it's in color box...
    if ($inrect($mouse.x,$mouse.y,82,$calc(51 + (%draw.height * %draw.gridy)),89,33) == $true) {
      set %draw.crow $int($calc((($mouse.x - 82) / 11)))
      set %draw.ccol $int($calc((($mouse.y - $calc(51 + (%draw.height * %draw.gridy))) / 16)))
      ;echo -s %draw.crow %draw.ccol = $calc((%draw.crow * 2) + %draw.ccol)
      set %draw.curcolor2 $calc((%draw.crow * 2) + %draw.ccol)
      draw.showcolor
      drawdot @ASCII Paint
    }
    else {
      set %draw.curcolortemp %draw.curcolor
      set %draw.curcolor %draw.curcolor2
      set %draw.curcolor2 %draw.curcolortemp
      unset %draw.curcolortemp 
      draw.showcolor
      drawdot @ASCII Paint
    }
  }
  sclick {
    if (%draw.nodraw) { halt }
    ; Check to see if it's in the grid...
    if ($inrect($mouse.x,$mouse.y,2,40,$calc((%draw.width * %draw.gridx)),$calc((%draw.height * %draw.gridy))) == $true) {
      set %draw.col $int($calc((($mouse.x - 2) / %draw.gridx) + 1))
      set %draw.row $int($calc((($mouse.y - 40) / %draw.gridy) + 1))
      ;echo -s > %draw.row %draw.col
      set %draw.col2 %draw.col
      set %draw.row2 %draw.row
      drawrect -f @ASCII Paint %draw.curcolor 1 $calc(3 - $iif(%draw.grid == 1,0,0) + ((%draw.col - 1) * %draw.gridx)) $calc(41 - $iif(%draw.grid == 1,0,0) + ((%draw.row - 1) * %draw.gridy)) $calc(%draw.gridx - $iif(%draw.grid == 1,1,0)) $calc(%draw.gridy - $iif(%draw.grid == 1,1,0))
      ;echo -s hadd -m draw %draw.row $iif(%draw.col > 1,$gettok($hget(draw,%draw.row),1- $+ $calc(%draw.col - 1),44) $+ $chr(44)) $+ %draw.curcolor $+ $iif(%draw.col < %draw.width,$chr(44) $+ $gettok($hget(draw,%draw.row),$calc(%draw.col + 1) $+ -,44))
      hadd -m draw %draw.row $iif(%draw.col > 1,$gettok($hget(draw,%draw.row),1- $+ $calc(%draw.col - 1),44) $+ $chr(44)) $+ %draw.curcolor $+ $iif(%draw.col < %draw.width,$chr(44) $+ $gettok($hget(draw,%draw.row),$calc(%draw.col + 1) $+ -,44))
    }
    ; Check if it's in the color box...
    if ($inrect($mouse.x,$mouse.y,82,$calc(51 + (%draw.height * %draw.gridy)),88,32) == $true) {
      set %draw.crow $int($calc((($mouse.x - 82) / 11)))
      set %draw.ccol $int($calc((($mouse.y - $calc(51 + (%draw.height * %draw.gridy))) / 16)))
      ;echo -s %draw.crow %draw.ccol = $calc((%draw.crow * 2) + %draw.ccol)
      set %draw.curcolor $calc((%draw.crow * 2) + %draw.ccol)
      draw.showcolor
      drawdot @ASCII Paint
    }
    ; Check if it's a button...
    set %x 1
    while (%x <= $hget(drawcmd,0).item) {
      set %y $hget(drawcmd,$hget(drawcmd,%x).item)
      if ($inrect($mouse.x,$mouse.y,$gettok(%y,1,32),$gettok(%y,2,32),$gettok(%y,3,32),$gettok(%y,4,32)) == $true) {
        draw.nomenu
        $gettok(%y,5-,32)
        halt
      }
      inc %x 1
    }
  }
}
alias -l draw.save-OLD {
  set %draw.tempfile $$sfile($iif(%draw.path,%draw.path,$mircdir),Save as...,Save)
  set %draw.path $nofile(%draw.tempfile)
  if ($exists(%draw.tempfile) == $true) {
    set %draw.temp $?!="File exists, overwrite?"
    if (%draw.temp == $false) { halt }
  }
  write -c " $+ %draw.tempfile $+ "
  write " $+ %draw.tempfile $+ " ASCII-PAINT SAVE FILE.
  set %x 1
  while (%x <= %draw.height) {
    write " $+ %draw.tempfile $+ " $hget(draw,%x)
    inc %x
  }

}
on 1:KEYDOWN:@ASCII Paint:*: {
  if ($keyval == 40) { 
    if (%draw.height < 60) {
      hadd -m draw $calc(%draw.height + 1) $str(0 $+ $chr(44),$calc(%draw.width - 1)) $+ 0
      inc %draw.height
      draw.regrid
      draw.showdim
      drawdot @ASCII Paint
    }
  }
  elseif ($keyval == 38) { 
    if (%draw.height > 1) {
      hdel draw %draw.height
      dec %draw.height
      draw.regrid
      draw.showdim
      drawdot @ASCII Paint
    }
  }
  elseif ($keyval == 37) { 
    if (%draw.width > 1) {
      set %x 1
      while (%x <= %draw.height) {
        hadd -m draw %x $gettok($hget(draw,%x),1- $+ $calc(%draw.width - 1),44)
        inc %x
      }
      dec %draw.width
      draw.regrid
      draw.showdim
      drawdot @ASCII Paint
    }
  }
  elseif ($keyval == 39) { 
    if (%draw.width < 90) {
      set %x 1
      while (%x <= %draw.height) {
        hadd -m draw %x $hget(draw,%x) $+ ,0
        inc %x
      }
      inc %draw.width
      draw.regrid
      draw.showdim
      drawdot @ASCII Paint
    }
  }
  elseif ($keychar == S) || ($keyrpt == 0) {
    ; Let's auto save the file to <mircdir>\autosave.draw
    draw.tip Autosaving...
    set %draw.spam autosave.draw
    write -c " $+ %draw.spam $+ "
    set %x 1
    while (%x <= %draw.height) {
      set %draw.spamtemp $replace($hget(draw,%x),$chr(44),$chr(32))
      unset %draw.spamtemp2
      unset %draw.spamtemp3
      set %y 1
      while (%y <= $numtok(%draw.spamtemp,32)) {
        if (%draw.spamtemp3 == $gettok(%draw.spamtemp,%y,32)) {
          set %draw.spamtemp2 %draw.spamtemp2 $+ X
          set %draw.spamtemp3 $gettok(%draw.spamtemp,%y,32)
        }
        else {
          set %draw.spamtemp2 %draw.spamtemp2 $+  $+ $gettok(%draw.spamtemp,%y,32) $+ , $+ $gettok(%draw.spamtemp,%y,32) $+ X
          set %draw.spamtemp3 $gettok(%draw.spamtemp,%y,32)
        }
        inc %y 1
      }
      write " $+ %draw.spam $+ " %draw.spamtemp2
      inc %x 1
    }
    draw.nomenu
    draw.tip Autosave complete.
  }
}
alias -l draw.open {
  set %draw.tempfile $$sfile($iif(%draw.path,%draw.path,$mircdir),Load file...,Load)
  set %draw.path $nofile(%draw.tempfile)
  if ($exists(%draw.tempfile) == $true) {
    if ($gettok(%draw.tempfile,$numtok(%draw.tempfile,46),46) == JPG) || ($gettok(%draw.tempfile,$numtok(%draw.tempfile,46),46) == BMP) || ($gettok(%draw.tempfile,$numtok(%draw.tempfile,46),46) == PNG) {
      draw.loadimage %draw.tempfile
      halt
    }
    hadd -m draw 1 hi
    hfree draw
    set %draw.tempheight $lines(%draw.tempfile)
    set %draw.tempwidth $len($strip($read(%draw.tempfile,nt,1)))
    ; Check to see if the width is the same the whole time.
    set %x 1
    while (%x <= $lines(%draw.tempfile)) {
      if (%draw.tempwidth != $len($strip($read(%draw.tempfile,nt,%x)))) || ($strip($read(%draw.tempfile,nt,1)) == $read(%draw.tempfile,nt,1)) {
        draw.tip Invalid file, loading halted.
        set %draw.curtip NONE
        set -u1 %draw.nodraw 1
        halt
      }
      inc %x 1
    }
    if (%draw.tempwidth < 1) || (%draw.tempwidth > 90) || (%draw.tempheight > 61) || (%draw.tempheight < 1) {
      ; invalid file, halt
      draw.tip Invalid file, loading halted.
      set %draw.curtip NONE
      set -u1 %draw.nodraw 1
      halt
    }
    else {
      set %draw.height %draw.tempheight
      set %draw.width %draw.tempwidth
    }
    draw
    set %x 1
    while (%x <= $lines(%draw.tempfile)) {
      hadd -m draw %x $draw.load.conv($read(%draw.tempfile,nt,%x))
      inc %x 1
    }
    set -u1 %draw.nodraw 1
    draw.draw
    titlebar @ASCII Paint - $nopath(%draw.tempfile)
    set %draw.send.DESC $iif($numtok($nopath(%draw.tempfile),46) > 1,$gettok($nopath(%draw.tempfile),$calc($numtok($nopath(%draw.tempfile),46) - 1),46),$nopath(%draw.tempfile))
    drawdot @ASCII Paint

  }
}
alias -l draw.load.conv {
  set %draw.temp $strip($$1-,bur)
  unset %draw.w
  set %draw.x 1
  while (%draw.x <= $numtok(%draw.temp,3)) {
    set %draw.y $gettok(%draw.temp,%draw.x,3)
    set %draw.z $gettok(%draw.y,1,44)
    set %draw.y $remove($gettok(%draw.y,2,44),1,2,3,4,5,6,7,8,9,0)
    set %draw.a 1
    while (%draw.a <= $len(%draw.y)) {
      if ($len(%draw.w) > 0) {
        set %draw.w %draw.w $+ , $+ %draw.z
      }
      else {
        set %draw.w %draw.z
      }
      inc %draw.a 1
    }
    inc %draw.x 1
  }  
  return %draw.w
}
alias -l draw.open-OLD {
  set %draw.tempfile $$sfile($iif(%draw.path,%draw.path,$mircdir),Load file...,Load)
  set %draw.path $nofile(%draw.tempfile)
  if ($exists(%draw.tempfile) == $true) {
    set %draw.x 2
    if ($read(%draw.tempfile,nt,1) != ASCII-PAINT SAVE FILE.) {
      if ($$?!="File is not valid save file, loading may have unpredictable results.  Continue?" == $false) { halt }
      else { set %draw.x 1 }
    }
    hadd -m draw 1 hi
    hfree draw
    set %draw.tempheight $calc($lines(%draw.tempfile) - %draw.x + 1)
    set %draw.tempwidth $numtok($read(%draw.tempfile,nt,%draw.x),44)
    if (%draw.tempwidth < 1) || (%draw.tempwidth > 90) || (%draw.tempheight > 61) || (%draw.tempheight < 1) {
      ; invalid file, halt
      halt
    }
    else {
      set %draw.height %draw.tempheight
      set %draw.width %draw.tempwidth
    }
    draw
    set %x %draw.x
    unset %draw.x
    while (%x <= $lines(%draw.tempfile)) {
      hadd -m draw $calc(%x - 1) $read(%draw.tempfile,nt,%x)
      inc %x 1
    }
    set -u1 %draw.nodraw 1
    draw.draw
    drawdot @ASCII Paint

  }
}
alias -l draw.regrid {
  window -af @ASCII Paint -1 -1 $iif($calc(5 + (%draw.width * %draw.gridx)) > 280, $calc(5 + (%draw.width * %draw.gridx)), 280) $calc(103 + (%draw.height * %draw.gridy))
  drawrect -fr @ASCII Paint %draw.color2 1 0 0 $window(@ASCII Paint).w $window(@ASCII Paint).h
  draw.mainmenu
  drawrect -nrf @ASCII Paint %draw.color2 1 2 40 $window(@ASCII Paint).w $window(@ASCII Paint).h
  if (%draw.grid == 1) {
    drawrect -nr @ASCII Paint %draw.color1 1 2 40 $calc(1 + (%draw.width * %draw.gridx)) $calc(1 + (%draw.height * %draw.gridy))
    set %x 1
    while (%x <= %draw.width) {
      drawline -nr @ASCII Paint %draw.color1 1 $calc(2 + (%x * %draw.gridx)) 40 $calc(2 + (%x * %draw.gridx)) $calc(40 + (%draw.height * %draw.gridy))
      inc %x 1
    }
    set %x 1
    while (%x <= %draw.height) {
      drawline -nr @ASCII Paint %draw.color1 1 2 $calc(40 + (%x * %draw.gridy)) $calc(2 + (%draw.width * %draw.gridx)) $calc(40 + (%x * %draw.gridy))
      inc %x 1
    }
  }
  draw.draw
  draw.colorbox
  draw.showcolor
  drawdot @ASCII Paint
}
alias -l draw.draw {
  set %x 1
  while (%x <= %draw.height) {
    set %y $hget(draw,%x)
    set %z 1
    while (%z <= %draw.width) {
      drawrect -nf @ASCII Paint $iif($gettok(%y,%z,44),$gettok(%y,%z,44),0) 1 $calc(3 + ($calc(%z - 1) * %draw.gridx)) $calc(41 + ($calc(%x - 1) * %draw.gridy)) $calc(%draw.gridx - $iif(%draw.grid == 1,1,0)) $calc(%draw.gridy - $iif(%draw.grid == 1,1,0))
      inc %z 1
    }
    inc %x 1
  }
}
alias -l draw.exit { window -c @ASCII Paint }
alias -l draw.spam {
  if ($window(@Spam Channel)) { window -c @Spam Channel }
  ; set %draw.spam $$input(Spam where?,5,ASCII Paint - Spam,%draw.spam.chan)
  set %draw.spam %draw.spam.chan
  set %draw.spam.chan %draw.spam
  set %x 1
  while (%x <= %draw.height) {
    set %draw.spamtemp $replace($hget(draw,%x),$chr(44),$chr(32))
    unset %draw.spamtemp2
    unset %draw.spamtemp3
    set %y 1
    while (%y <= $numtok(%draw.spamtemp,32)) {
      if (%draw.spamtemp3 == $gettok(%draw.spamtemp,%y,32)) {
        set %draw.spamtemp2 %draw.spamtemp2 $+ X
        set %draw.spamtemp3 $gettok(%draw.spamtemp,%y,32)
      }
      else {
        set %draw.spamtemp2 %draw.spamtemp2 $+  $+ $gettok(%draw.spamtemp,%y,32) $+ , $+ $gettok(%draw.spamtemp,%y,32) $+ X
        set %draw.spamtemp3 $gettok(%draw.spamtemp,%y,32)
      }
      inc %y 1
    }
    .timer -m 1 $calc(((%x - 1) * %draw.delay) + 1) MSG %draw.spam %draw.spamtemp2
    inc %x 1
  }
}
alias -l draw.rotate {
  hadd -m draw2 1 hi
  hfree draw2
  set %x $hget(draw,0).item
  while (%x > 0) {
    set %z $hget(draw,%x)
    set %y $numtok(%z,44)
    while (%y > 0) {
      set %w $hget(draw2,%y)
      if ($len(%w) > 0) {
        hadd -m draw2 %y %w $+ , $+ $gettok(%z,%y,44)
      }
      else {
        hadd -m draw2 %y $gettok(%z,%y,44)
      }
      dec %y 1
    }
    dec %x 1
  }
  set %x 1
  hadd -m draw 1 hi
  hfree draw
  while (%x <= $hget(draw2,0).item) {
    hadd -m draw %x $hget(draw2,%x) 
    inc %x 1
  }
  set %draw.temp %draw.width
  set %draw.width %draw.height
  set %draw.height %draw.temp
  unset %draw.temp
  draw.regrid
  draw.showcolor
  draw.showdim
  drawdot @ASCII Paint
}
alias -l draw.flip {
  set %x 1
  hadd -m draw2 1 hi
  hfree draw2
  while (%x <= $hget(draw,0).item) {
    hadd -m draw2 %x $hget(draw,%x)
    inc %x 1
  }
  hfree draw
  set %x $hget(draw2,0).item
  set %y 1
  while (%x > 0) {
    hadd -m draw %y $hget(draw2,%x)
    inc %y 1
    dec %x 1
  }
  draw.draw
  drawdot @ASCII Paint
}
alias -l draw.mirror {
  set %z 1
  while (%z <= %draw.height) {
    hadd -m draw %z $draw.reverse($hget(draw,%z))
    inc %z 1
  }
  draw.draw
  drawdot @ASCII Paint
}
alias -l draw.reverse {
  set %x $numtok($$1-,44)
  unset %y
  while (%x > 0) {
    if ($len(%y) > 0) {
      set %y %y $+ , $+ $gettok($$1-,%x,44)
    }
    else {
      set %y $gettok($$1-,%x,44)
    }

    dec %x 1
  }
  return %y
}
alias draw.temp {
  set %x 0
  while (%x <= 15) {
    inc %x 1
  }
}
alias -l draw.delay {
  draw.nomenu
  unset %draw.delaytemp
  set %draw.delaytemp $$input(How many second delay for spam? Default=3,5,ASCII Paint - Spam Delay,%draw.delay)
  if (%draw.delaytemp isnum) && (%draw.delaytemp > 0) { set %draw.delay %draw.delaytemp }
  else { set %draw.delay 3 }
}
alias -l draw.fill {
  set %x 1
  while (%x <= %draw.height) {
    set %y 1
    while (%y <= %draw.width) {
      hadd -m draw %x $reptok($hget(draw,%x),%draw.curcolor2,%draw.curcolor,1,44)
      inc %y 1
    }
    inc %x 1
  }
  draw.draw
  drawdot @ASCII Paint
}
alias -l draw.save {
  set %draw.spam $$sfile($iif(%draw.path,%draw.path,$mircdir),Save as...,Save)
  set %draw.path $nofile(%draw.spam)
  if ($exists(%draw.spam) == $true) {
    if ($$?!="File exists, overwrite?" == $true) { write -c " $+ %draw.spam $+ " }
    else { halt }
  }
  write -c " $+ %draw.spam $+ "
  set %x 1
  while (%x <= %draw.height) {
    set %draw.spamtemp $replace($hget(draw,%x),$chr(44),$chr(32))
    unset %draw.spamtemp2
    unset %draw.spamtemp3
    set %y 1
    while (%y <= $numtok(%draw.spamtemp,32)) {
      if (%draw.spamtemp3 == $gettok(%draw.spamtemp,%y,32)) {
        set %draw.spamtemp2 %draw.spamtemp2 $+ X
        set %draw.spamtemp3 $gettok(%draw.spamtemp,%y,32)
      }
      else {
        set %draw.spamtemp2 %draw.spamtemp2 $+  $+ $gettok(%draw.spamtemp,%y,32) $+ , $+ $gettok(%draw.spamtemp,%y,32) $+ X
        set %draw.spamtemp3 $gettok(%draw.spamtemp,%y,32)
      }
      inc %y 1
    }
    write " $+ %draw.spam $+ " %draw.spamtemp2
    inc %x 1
  }
  titlebar @ASCII Paint - $nopath(%draw.spam)
  set %draw.send.DESC $iif($numtok($nopath(%draw.spam),46) > 1,$gettok($nopath(%draw.spam),$calc($numtok($nopath(%draw.spam),46) - 1),46),$nopath(%draw.spam))
  draw.nomenu
}
alias -l draw.echo {
  set %x 1
  while (%x <= %draw.height) {
    set %draw.spamtemp $replace($hget(draw,%x),$chr(44),$chr(32))
    unset %draw.spamtemp2
    unset %draw.spamtemp3
    set %y 1
    while (%y <= $numtok(%draw.spamtemp,32)) {
      if (%draw.spamtemp3 == $gettok(%draw.spamtemp,%y,32)) {
        set %draw.spamtemp2 %draw.spamtemp2 $+ X
        set %draw.spamtemp3 $gettok(%draw.spamtemp,%y,32)
      }
      else {
        set %draw.spamtemp2 %draw.spamtemp2 $+  $+ $gettok(%draw.spamtemp,%y,32) $+ , $+ $gettok(%draw.spamtemp,%y,32) $+ X
        set %draw.spamtemp3 $gettok(%draw.spamtemp,%y,32)
      }
      inc %y 1
    }
    echo -s %draw.spamtemp2
    inc %x 1
  }
  draw.nomenu
  unset %draw.spamtemp*
}
alias -l draw.gridopt {
  set %draw.grid $$?!="Display the grid?"
  draw.regrid
}
alias -l draw.gridsize {
  set %draw.gridx $$input(Grid width? Default=11,5,ASCII Paint - Grid Size,%draw.gridx)
  set %draw.gridy $$input(Grid height? Default=16,5,ASCII Paint - Grid Size,%draw.gridy)
  draw.regrid
}
alias -l draw.send {
  if ($window(@Send Image)) { window -c @Send Image }
  set -u1 %draw.nodraw 1
  ;set %draw.sendto $$input(Send to who $chr(40) $+ irc nick $+ $chr(41) $+ ?,5,ASCII Paint - Send Image,%draw.sendto)
  set %draw.sendto %draw.send.nick
  if (%draw.sendto == $null) { halt }
  if ($sock(drawlisten)) { sockclose drawlisten }
  socklisten drawlisten
  set %draw.sendpass $base($ticks,10,32) $+ $base($rand(1,999999),10,32)
  .CTCP %draw.sendto ASCII $IP $sock(drawlisten).port %draw.sendpass %draw.width %draw.height %draw.gridx %draw.gridy %draw.grid %draw.send.desc
  if ($exists(draw.sendto.txt) == $true) { .remove draw.sendto.txt }
  set %draw.spam draw.sendto.txt
  set %draw.path $nofile(%draw.spam)
  if ($exists(%draw.spam) == $true) {
    if ($$?!="File exists, overwrite?" == $true) { write -c " $+ %draw.spam $+ " }
    else { halt }
  }
  write -c " $+ %draw.spam $+ "
  set %x 1
  while (%x <= %draw.height) {
    set %draw.spamtemp $replace($hget(draw,%x),$chr(44),$chr(32))
    unset %draw.spamtemp2
    unset %draw.spamtemp3
    set %y 1
    while (%y <= $numtok(%draw.spamtemp,32)) {
      if (%draw.spamtemp3 == $gettok(%draw.spamtemp,%y,32)) {
        set %draw.spamtemp2 %draw.spamtemp2 $+ X
        set %draw.spamtemp3 $gettok(%draw.spamtemp,%y,32)
      }
      else {
        set %draw.spamtemp2 %draw.spamtemp2 $+  $+ $gettok(%draw.spamtemp,%y,32) $+ , $+ $gettok(%draw.spamtemp,%y,32) $+ X
        set %draw.spamtemp3 $gettok(%draw.spamtemp,%y,32)
      }
      inc %y 1
    }
    write " $+ %draw.spam $+ " %draw.spamtemp2
    inc %x 1
  }
  draw.tip Sending request to %draw.sendto $+ ...
  set %draw.curtip NONE
}
on 1:socklisten:drawlisten: {
  if ($sockerr > 0) { return }
  set %draw.temp draws. $+ $ticks
  sockaccept %draw.temp
  .timer $+ %draw.temp 1 300 sockclose %draw.temp
}
on 1:sockread:draws.*: {
  if ($sockerr > 0) { return }
  sockread %temp
  if (%temp == %draw.sendpass) {
    set %draw.sendpass $base($ticks,10,32) $+ $base($rand(1,999999),10,32)
    if ($exists(draw.sendto.txt) == $true) {
      set %draw.tempfile draw.sendto.txt
      set %draw.path $nofile(%draw.tempfile)
      if ($exists(%draw.tempfile) == $true) {
        set %draw.tempheight $lines(%draw.tempfile)
        set %draw.tempwidth $len($strip($read(%draw.tempfile,nt,1)))
        ; Check to see if the width is the same the whole time.
        set %x 1
        while (%x <= $lines(%draw.tempfile)) {
          if (%draw.tempwidth != $len($strip($read(%draw.tempfile,nt,%x)))) || ($strip($read(%draw.tempfile,nt,1)) == $read(%draw.tempfile,nt,1)) {
            sockwrite -n $sockname >>>CANCELLED
            .remove draw.sendto.txt
            .sockclose drawlisten
            .timer $+ $sockname off
            set %draw.curtip NONE
            set -u1 %draw.nodraw 1
            halt
          }
          inc %x 1
        }
        if (%draw.tempwidth < 1) || (%draw.tempwidth > 150) || (%draw.tempheight > 150) || (%draw.tempheight < 1) {
          ; invalid file, halt
          sockwrite -n $sockname >>>CANCELLED
          .remove draw.sendto.txt
          .sockclose drawlisten
          .timer $+ $sockname off
          set %draw.curtip NONE
          set -u1 %draw.nodraw 1
          halt
        }
        else {
          sockwrite -n $sockname >>>DIM %draw.tempheight %draw.tempwidth
        }
        set %x 1
        while (%x <= $lines(%draw.tempfile)) {
          sockwrite -n $sockname >>>HASH %x $draw.load.conv($read(%draw.tempfile,nt,%x))
          inc %x 1
        }
        sockwrite -n $sockname >>>FINISHED
        set %draw.sendpass $base($ticks,10,32) $+ $base($rand(1,999999),10,32)
        .sockclose drawlisten
        .remove draw.sendto.txt
        .timer $+ $sockname off
        draw.tip Image sent successfully.
      }

    }
  }
  else {
    sockwrite -n $sockname >>>BADAUTH
    .timer $+ $sockname off
  }
}

CTCP *:ASCII:?: {
  if ($4 == $null) { halt }
  ;.CTCP %draw.sendto ASCII $IP $sock(drawlisten).port %draw.sendpass %draw.width %draw.height %draw.gridx %draw.gridy %draw.send.desc
  set %draw.temp.get.nick $fulladdress
  set %draw.temp.get.ip $$2
  set %draw.temp.get.port $$3
  set %draw.temp.get.pass $$4
  set %draw.temp.get.dim $$5-9
  unset %draw.temp.get.desc
  set %draw.temp.get.desc $10-
  draw.getwindow
  haltdef
  halt
}

alias draw-get {
  set -u1 %draw.nodraw 1
  if ($window(@Receive Image)) { window -c @Receive Image }
  if (%draw.temp.get.usedim == $true) {
    set %draw.gridx $gettok(%draw.temp.get.dim,3,32)
    set %draw.gridy $gettok(%draw.temp.get.dim,4,32)
    set %draw.grid $gettok(%draw.temp.get.dim,5,32)
    ; echo -s > %draw.grid - $gettok(%draw.temp.get.dim,5,32)
  }
  set %draw.get.ip %draw.temp.get.ip
  set %draw.get.port %draw.temp.get.port
  set %draw.get.pass %draw.temp.get.pass

  draw
  hadd -m draw 1 hi
  hfree -w draw
  set %draw.nodraw 1
  draw.tip Connecting...
  if ($sock(draw.get)) { sockclose draw.get }
  sockopen draw.get %draw.get.ip %draw.get.port
  .timerdraw.getstop 1 15 draw.getstop
}
on 1:sockopen:draw.get: {
  if ($sockerr > 0) { 
    draw.tip $sock($sockname).wsmsg 
    set %draw.nodraw 0
    return 
  }
  sockwrite -n $sockname %draw.get.pass
  .timerdraw.getstop 1 30 draw.getstop
  draw.tip Connected, validating...
  set %draw.dots ...
}
on 1:sockclose:draw.get: {
  if ($sockerr > 0) { return }
  set %draw.nodraw 0
}
on 1:sockread:draw.get: {
  if ($sockerr > 0) { return }
  sockread %temp
  tokenize 32 %temp
  draw.tip Receiving data $+ %draw.dots
  set %draw.curtip NONE
  if ($len(%draw.dots == 1)) { set %draw.temp.dots .. }
  if ($len(%draw.dots == 2)) { set %draw.temp.dots ... }
  if ($len(%draw.dots == 3)) { set %draw.temp.dots . }
  set %draw.dots %draw.temp.dots
  unset %draw.temp.dots
  if ($$1 == >>>DIM) {
    if ($$2 < 150) && ($$3 < 150) {
      set %draw.height $$2
      set %draw.width $$3
      draw.regrid
      drawdot @ASCII Paint
    }
  }
  elseif ($$1 == >>>HASH) {
    hadd -m draw $$2-
  }
  elseif ($$1 == >>>FINISHED) {
    draw.draw
    draw.showdim
    drawdot @ASCII PAINT
    draw.tip Drawing received.
    set %draw.nodraw 0
    sockclose $sockname
    .timerdraw.getstop off
  }
  elseif ($$1 == >>>BADAUTH) {
    draw.tip Bad authentication.
    sockclose $sockname
    .timerdraw.getstop off

  }
  elseif ($$1 == >>>CANCELLED) {
    draw.tip Send cancelled.
    sockclose $sockname
    set %draw.nodraw 0
    .timerdraw.getstop off

  }
}
alias draw.getstop {
  if ($sock(draw.get)) {
    draw.tip Socket timed out...
    draw
    sockclose draw.get
    set %draw.nodraw 0
  }
}


alias gui.init {
  hadd -m gui.cmd. $+ $$1 1 hi
  hfree -w gui.*. $+ $$1
}
alias gui.set.cmd {
  ; set.CMD Format: gui.set.cmd @WINDOW # Command <Parmameters>
  hadd -m gui.cmd. $+ $$1 $$2 $$3-
}
alias gui.button.txt {
  ; Tokenize the control date, except for the current text.
  tokenize 32 $$1 $$2 $gettok($hget(gui.controls. $+ $$1,$$2),2-5,32) $3-
  hadd -m gui.controls. $+ $$1 $$2 BUTTON $$3-
  drawrect -nrf $$1 %draw.color2 1 $$3-6
  drawrect -nr $$1 %draw.color1 1 $$3-6
  if ($7) {
    drawtext -ncr $$1 %draw.color1 Fixedsys 14 $round($calc(($$3 + ($$5 / 2)) - ($width($$7-,fixedsys,14) / 2)),0) $round($calc(($$4 + ($$6 / 2)) - ($height($$7-,Fixedsys,14) / 2)),0) $width($$7-,Fixedsys,14) $height($$7-,Fixedsys,14) $7-
  }
  drawdot $$1
}
alias gui.button {
  ; Button format: gui.button @WINDOW # X Y W H Text
  ; Button-CMD: gui.cmd @Window # Command
  if ($window($$1)) {
    hadd -m gui.controls. $+ $$1 $$2 BUTTON $$3-6 $7-
    drawrect -nrf $$1 %draw.color2 1 $$3-6
    drawrect -nr $$1 %draw.color1 1 $$3-6
    drawtext -ncr $$1 %draw.color1 Fixedsys 14 $round($calc(($$3 + ($$5 / 2)) - ($width($$7-,fixedsys,14) / 2)),0) $round($calc(($$4 + ($$6 / 2)) - ($height($$7-,Fixedsys,14) / 2)),0) $width($$7-,Fixedsys,14) $height($$7-,Fixedsys,14) $$7-
    drawdot $$1
  }
  else {
  }
}
alias gui.edit {
  ; Editbox format: gui.edit @WINDOW # X Y W H MAXLEN VARIABLE TEXT
  ; Variable DOESNOT get the % in the line.  So the variable "%TEMP" would just be TEMP
  drawrect -nrf $$1 $gui.rgbmorph(%draw.color2) 1 $$3-6
  drawrect -nr $$1 %draw.color1 1 $$3-6
  hadd -m gui.controls. $+ $$1 $$2 editbox $$3-8
  if ($8) {
    set % $+ [ $$8 ] $9-
  }
  set %draw.gui.temp.curtext $9-
  set %y editbox $$3-8
  if ($len(%draw.gui.temp.curtext) > 0) {
    drawtext -nrc $$1 %draw.color1 Fixedsys 14 $calc($gettok(%y,2,32) + 2) $calc($gettok(%y,3,32) + 2) $calc($gettok(%y,4,32) - 4) $calc($gettok(%y,5,32) - 4) %draw.gui.temp.curtext
  }
  drawdot $$1
  ;set %draw.gui.temp. $+ $$
}
alias -l gui.rgbmorph {
  if ($chr(44) isin $$1) {
    tokenize 44 $$1
  }
  else {
    tokenize 44 $rgb($$1)
  }
  return $rgb($int($calc($$1 + ((255 - $$1) / 2))),$int($calc($$2 + ((255 - $$2) / 2))),$int($calc($$3 + ((255 - $$3) / 2))))
}
alias gui.process.sclick {
  set %x 1
  while (%x <= $hget(gui.controls. $+ $$1,0).item) {
    set %y $hget(gui.controls. $+ $$1,%x)
    if ($inrect($$2,$$3,$gettok(%y,2,32),$gettok(%y,3,32),$gettok(%y,4,32),$gettok(%y,5,32)) == $true) {
      if ($gettok(%y,1,32) == editbox) {
        ; Clicked control is an editbox.
        set %draw.gui.temp.curtext [ % $+ [ $gettok(%y,7,32) ] ]
        set %draw.gui.temp.curvar $gettok(%y,7,32)
        set %draw.gui.temp.maxlen $gettok(%y,6,32)
        drawtext -nrc $$1 %draw.color1 Fixedsys 14 $calc($gettok(%y,2,32) + 2) $calc($gettok(%y,3,32) + 2) $calc($gettok(%y,4,32) - 4) $calc($gettok(%y,5,32) - 4) %draw.gui.temp.curtext $+ _
        drawdot $$1
        gui.setfocus $$1 %x
        halt
      }
      elseif ($gettok(%y,1,32) == button) {
        ; Clicked control is a button. Execute the given command.
        gui.setfocus $$1 %x
        $hget(gui.cmd. $+ $$1,%x)
        halt
      }
    }
    else {
      gui.setfocus $$1 NONE
    }
    inc %x 1
  }
}
alias gui.forcefocus {
  set %y $hget(gui.controls. $+ $$1,$$2)
  if ($gettok(%y,1,32) == EDITBOX) {
    set %draw.gui.temp.curtext [ % $+ [ $gettok(%y,7,32) ] ]
    set %draw.gui.temp.curvar $gettok(%y,7,32)
    set %draw.gui.temp.maxlen $gettok(%y,6,32)
    drawtext -nrc $$1 %draw.color1 Fixedsys 14 $calc($gettok(%y,2,32) + 2) $calc($gettok(%y,3,32) + 2) $calc($gettok(%y,4,32) - 4) $calc($gettok(%y,5,32) - 4) %draw.gui.temp.curtext $+ _
    drawdot $$1
    gui.setfocus $$1 $$2
  }
  elseif ($gettok(%y,1,32) == BUTTON) { 
    drawrect -nrf $gettok(%draw.gui.temp.curfocus,1,32) %draw.color1 1 $gettok(%y,2-5,32)
    drawrect -nr $gettok(%draw.gui.temp.curfocus,1,32) %draw.color2 1 $gettok(%y,2-5,32)
    drawtext -ncr $gettok(%draw.gui.temp.curfocus,1,32) %draw.color2 Fixedsys 14 $round($calc(($gettok(%y,2,32) + ($gettok(%y,4,32) / 2)) - ($width($gettok(%y,6-,32),fixedsys,14) / 2)),0) $round($calc(($gettok(%y,3,32) + ($gettok(%y,5,32) / 2)) - ($height($gettok(%y,6-,32),Fixedsys,14) / 2)),0) $width($gettok(%y,6-,32),Fixedsys,14) $height($gettok(%y,6-,32),Fixedsys,14) $gettok(%y,6-,32)
    drawdot $gettok(%draw.gui.temp.curfocus,1,32)
    gui.setfocus $$1 $$2 
  }

}
alias gui.process.keydown {
  if ($$2 == 8) && ($$3 == NORMAL) {
    ; Backspace, delete a letter.
    set %draw.gui.temp.curtext $left(%draw.gui.temp.curtext, $calc($len(%draw.gui.temp.curtext) - 1))
    set % $+ [ %draw.gui.temp.curvar ] %draw.gui.temp.curtext
    gui.redrawcurrenteditbox $$1
  }
  elseif ($$3 == NORMAL) && ($$2 == 32) && ($len(%draw.gui.temp.curtext) < %draw.gui.temp.maxlen) {
    ; Space key, just give it a 160 instead.  (Helps with multiple spaces)
    set %draw.gui.temp.curtext %draw.gui.temp.curtext $+ $chr(160)
    set % $+ [ %draw.gui.temp.curvar ] %draw.gui.temp.curtext
    gui.redrawcurrenteditbox $$1
  }
  elseif ($$3 == NORMAL) && ($$2 == 9) { 
    ; Process the TAB key.  (Move focus to the next control)
    ; check if we have a focused control to begin with...
    if ($gettok(%draw.gui.temp.curfocus,2,32) != NONE) {
      gui.forcefocus $gettok(%draw.gui.temp.curfocus,1,32) $iif($calc($gettok(%draw.gui.temp.curfocus,2,32) + 1) > $hget(gui.controls. $+ $gettok(%draw.gui.temp.curfocus,1,32),0).item,1,$calc($gettok(%draw.gui.temp.curfocus,2,32) + 1))
    }
    else {
      ; No control focused on, set focus on control #1.
      gui.forcefocus $gettok(%draw.gui.temp.curfocus,1,32) 1
    }
  }
  elseif ($$3 == NORMAL) && ($$2 == 13) {
    ; Process the ENTER key.  (Execute a command bound to the current active control, if there is one)
    $hget(gui.cmd. $+ $gettok(%draw.gui.temp.curfocus,1,32),$gettok(%draw.gui.temp.curfocus,2,32))
  }
  else {
    if ($$3 == NORMAL) && ($len($4 > 0)) && ($len(%draw.gui.temp.curtext) < %draw.gui.temp.maxlen)  {
      set %draw.gui.temp.curtext %draw.gui.temp.curtext $+ $$4
      set % $+ [ %draw.gui.temp.curvar ] %draw.gui.temp.curtext
      gui.redrawcurrenteditbox $$1
    }
  }
}
alias -l gui.setfocus {
  ;         set %draw.gui.temp.curfocus $$1 %x
  if ($$1- != %draw.gui.temp.curfocus) {
    if ($window($gettok(%draw.gui.temp.curfocus,1,32))) { 
      set %y $hget(gui.controls. $+ $gettok(%draw.gui.temp.curfocus,1,32),$gettok(%draw.gui.temp.curfocus,2,32))
      if ($gettok(%y,1,32) == EDITBOX) {
        drawrect -nrf $gettok(%draw.gui.temp.curfocus,1,32) $gui.rgbmorph(%draw.color2) 1 $gettok(%y,2-5,32)
        drawrect -nr $gettok(%draw.gui.temp.curfocus,1,32) %draw.color1 1 $gettok(%y,2-5,32)
        if ($len([ % $+ [ $gettok(%y,7,32) ] ]) > 0) {
          drawtext -nrc $gettok(%draw.gui.temp.curfocus,1,32) %draw.color1 Fixedsys 14 $calc($gettok(%y,2,32) + 2) $calc($gettok(%y,3,32) + 2) $calc($gettok(%y,4,32) - 4) $calc($gettok(%y,5,32) - 4) [ % $+ [ $gettok(%y,7,32) ] ]
        }
        drawdot $gettok(%draw.gui.temp.curfocus,1,32)
      }
      elseif ($gettok(%y,1,32) == BUTTON) {
        ;echo -s -> %y
        drawrect -nrf $gettok(%draw.gui.temp.curfocus,1,32) %draw.color2 1 $gettok(%y,2-5,32)
        drawrect -nr $gettok(%draw.gui.temp.curfocus,1,32) %draw.color1 1 $gettok(%y,2-5,32)
        drawtext -ncr $gettok(%draw.gui.temp.curfocus,1,32) %draw.color1 Fixedsys 14 $round($calc(($gettok(%y,2,32) + ($gettok(%y,4,32) / 2)) - ($width($gettok(%y,6-,32),fixedsys,14) / 2)),0) $round($calc(($gettok(%y,3,32) + ($gettok(%y,5,32) / 2)) - ($height($gettok(%y,6-,32),Fixedsys,14) / 2)),0) $width($gettok(%y,6-,32),Fixedsys,14) $height($gettok(%y,6-,32),Fixedsys,14) $gettok(%y,6-,32)
        drawdot $gettok(%draw.gui.temp.curfocus,1,32)
      }
    }
    set %draw.gui.temp.curfocus $$1-
  }
}
alias gui.redrawcurrenteditbox {
  if ($gettok(%draw.gui.temp.curfocus,1,32) == $$1) {
    set %y $hget(gui.controls. $+ $gettok(%draw.gui.temp.curfocus,1,32),$gettok(%draw.gui.temp.curfocus,2,32))
    if ($gettok(%y,1,32) == EDITBOX) {
      drawrect -nrf $gettok(%draw.gui.temp.curfocus,1,32) $gui.rgbmorph(%draw.color2) 1 $gettok(%y,2-5,32)
      drawrect -nr $gettok(%draw.gui.temp.curfocus,1,32) %draw.color1 1 $gettok(%y,2-5,32)
      drawtext -nrc $gettok(%draw.gui.temp.curfocus,1,32) %draw.color1 Fixedsys 14 $calc($gettok(%y,2,32) + 2) $calc($gettok(%y,3,32) + 2) $calc($gettok(%y,4,32) - 4) $calc($gettok(%y,5,32) - 4) %draw.gui.temp.curtext $+ _
      drawdot $gettok(%draw.gui.temp.curfocus,1,32)
    }
  }
}
alias draw.gridoptions {
  gui.init @Paint Options
  window -aBdfhiknop +fnt @Paint Options -1 -1 210 155
  window -a @Paint Options
  drawfill -nr @Paint Options %draw.color2 %draw.color2 1 1
  drawtext -ncr @Paint Options %draw.color1 Fixedsys 14 2 2 90 20 Grid:
  drawtext -nrc @Paint Options %draw.color1 Fixedsys 14 10 20 50 20 Width:
  gui.edit @Paint Options 1 60 20 28 20 2 DRAW.GRIDX %draw.gridx
  gui.set.cmd @Paint Options 1 draw.gridoptions.close
  drawtext -nrc @Paint Options %draw.color1 Fixedsys 14 106 20 58 20 Height:
  gui.edit @Paint Options 2 164 20 28 20 2 DRAW.GRIDY %draw.gridy
  gui.set.cmd @Paint Options 2 draw.gridoptions.close
  drawtext -ncr @Paint Options %draw.color1 Fixedsys 14 2 52 190 20 Image Dimensions:
  drawtext -nrc @Paint Options %draw.color1 Fixedsys 14 10 70 50 20 Width:
  gui.edit @Paint Options 3 60 70 36 20 3 DRAW.WIDTH %draw.WIDTH
  gui.set.cmd @Paint Options 3 draw.gridoptions.close
  drawtext -nrc @Paint Options %draw.color1 Fixedsys 14 106 70 58 20 Height:
  gui.edit @Paint Options 4 164 70 36 20 3 DRAW.HEIGHT %draw.HEIGHT
  gui.set.cmd @Paint Options 4 draw.gridoptions.close
  gui.button @Paint Options 5 10 100 190 20 $iif(%draw.grid == 1,Disable,Enable) Grid
  gui.set.cmd @Paint Options 5 draw.buttonenablegridtoggle
  drawline -nr @Paint Options %draw.color1 1 10 125 200 125
  gui.button @Paint Options 6 10 130 190 20 Ok
  gui.set.cmd @Paint Options 6 draw.gridoptions.close
  drawdot @Paint Options
  gui.forcefocus @Paint Options 1
}
menu @Paint Options {
  sclick gui.process.sclick @Paint Options $mouse.x $mouse.y
}
on 1:keydown:@Paint Options:*: {
  if ($keyval == 255) { set %draw.gui.keyval.wait 1 | halt }
  if (%draw.gui.keyval.wait == 1) { gui.process.keydown @Paint Options $keyval ALT $keychar | set %draw.gui.keyval.wait 0 }
  else { 
    if ($keychar isnum 0-) || ($keyval == 8) || ($keyval == 13) || ($keyval == 9) { gui.process.keydown @Paint Options $keyval NORMAL $keychar } 
  }
}
alias draw.buttonenablegridtoggle {
  set %draw.temp.grid %draw.grid
  if (%draw.temp.grid == 1) { set %draw.grid 0 }
  else { set %draw.grid 1 }
  unset %draw.temp.grid
  gui.button.txt @Paint Options 5 $iif(%draw.grid == 1,Disable,Enable) Grid
}
on 1:CLOSE:@Paint Options: {
  if ($window(@Ascii PAINT)) {
    set -u1 %draw.nodraw 1
    if (%draw.height < 1) { set %draw.height 1 }
    if (%draw.width < 1) { set %draw.width 1 }
    if (%draw.gridx < 1) { set %draw.gridx 1 }
    if (%draw.gridy < 1) { set %draw.gridy 1 }
    draw.fixhash
    draw.regrid
    draw.showdim
    drawdot @ASCII Paint
  }
}
alias -l draw.gridoptions.close {
  hfree -w gui.*.@Paint Options
  window -c @Paint Options
  if ($window(@Ascii PAINT)) {
    set -u1 %draw.nodraw 1
    if (%draw.height < 1) { set %draw.height 1 }
    if (%draw.width < 1) { set %draw.width 1 }
    if (%draw.gridx < 1) { set %draw.gridx 1 }
    if (%draw.gridy < 1) { set %draw.gridy 1 }
    draw.fixhash
    draw.regrid
    draw.showdim
    drawdot @ASCII Paint
  }
}
alias draw.sendto {
  gui.init @Send Image
  window -aBdfhiknop +fnt @Send Image -1 -1 320 155
  window -a @Send Image
  drawfill -nr @Send Image %draw.color2 %draw.color2 1 1
  drawtext -ncr @Send Image %Draw.color1 Fixedsys 14 10 10 200 20 Send to (IRC Nickname):
  gui.edit @Send Image 1 10 40 300 20 36 DRAW.SEND.NICK %draw.send.nick
  gui.set.cmd @Send Image 1 draw.send
  drawtext -ncr @Send Image %draw.color1 Fixedsys 14 10 70 200 20 Image description:
  gui.edit @Send Image 2 10 100 300 20 36 DRAW.SEND.DESC %draw.send.DESC
  gui.set.cmd @Send Image 2 draw.send
  gui.button @Send Image 3 10 130 300 20 Send!
  gui.set.cmd @Send Image 3 draw.send
  gui.forcefocus @Send Image 1
}
menu @Send Image {
  sclick gui.process.sclick @Send Image $mouse.x $mouse.y
}
on 1:keydown:@Send Image:*: {
  if ($keyval == 255) { set %draw.gui.keyval.wait 1 | halt }
  if (%draw.gui.keyval.wait == 1) { gui.process.keydown @Send Image $keyval ALT $keychar | set %draw.gui.keyval.wait 0 }
  else { 
    gui.process.keydown @Send Image $keyval NORMAL $keychar 
  }
}
alias draw.getwindow {
  if ($window(@Receive Image)) { window -c @Receive Image }
  gui.init @Send Image
  window -aBfiknp +fnt @Receive Image -1 -1 320 220
  window -a @Receive Image
  drawfill -nr @Receive Image %draw.color2 %draw.color2 1 1
  drawtext -ncr @Receive Image %Draw.color1 Fixedsys 14 1 2 300 20 Image send requested from:
  drawtext -ncr @Receive Image %draw.color1 Fixedsys 14 10 22 300 20 %draw.temp.get.nick
  drawtext -ncr @Receive Image %Draw.color1 Fixedsys 14 1 42 300 20 Description:
  drawtext -ncr @Receive Image %Draw.color1 Fixedsys 14 10 62 300 20 $iif(%draw.temp.get.desc,%draw.temp.get.desc,None provided)
  drawtext -ncr @Receive Image %Draw.color1 Fixedsys 14 1 82 300 20 IP/Port:
  drawtext -ncr @Receive Image %Draw.color1 Fixedsys 14 10 102 300 20 %draw.temp.get.ip $+ : $+ %draw.temp.get.port
  drawtext -ncr @Receive Image %Draw.color1 Fixedsys 14 1 122 300 20 Dimensions:
  drawtext -ncr @Receive Image %Draw.color1 Fixedsys 14 10 142 300 20 Image: $gettok(%draw.temp.get.dim,1,32) $+ x $+ $gettok(%draw.temp.get.dim,2,32) Grid: $gettok(%draw.temp.get.dim,3,32) $+ x $+ $gettok(%draw.temp.get.dim,4,32) Grid is $iif($gettok(%draw.temp.get.dim,5,32) == $true,on,off)
  drawline -ncr @Receive Image %draw.color1 1 3 187 317 187
  set %draw.temp.get.usedim $false
  gui.button @Receive Image 1 10 162 300 20 Use custom dimensions
  gui.set.cmd @Receive Image 1 draw.toggle.customdim
  gui.button @Receive Image 2 10 192 100 20 Get
  gui.set.cmd @Receive Image 2 draw-get
  gui.button @Receive Image 3 210 192 100 20 Reject
  gui.set.cmd @Receive Image 3 window -c @Receive Image
}
menu @Receive Image {
  sclick gui.process.sclick @Receive Image $mouse.x $mouse.y
}
on 1:unload: {
  echo $color(info) -s *** Unloading ASCII-Paint...
  if ($window(@ASCII Paint)) { window -c @ASCII Paint }
  if ($window(@Receive Image)) { window -c @Receive Image }
  if ($window(@Send Image)) { window -c @Send Image }
  if ($window(@Paint Options)) { window -c @Paint Options }
  echo $color(info) -si2 --- Removed: open windows.
  unset %draw.*
  echo $color(info) -si2 --- Removed: variables.
  hfree -w gui.*
  hfree -w draw*
  echo $color(info) -si2  --- Removed: hash tables.
  echo $color(info) -s *** Unload complete.  To load at a later time: /load -rs $script
}
on 1:load: {
  echo $color(info) -s *** Initializing ASCII-Paint
  unset %draw.*
  set %draw.curcolor 1
  set %draw.curcolor2 0
  set %draw.color1 0
  set %draw.color2 $rgb(208,208,208)
  set %draw.height 10
  set %draw.width 30
  set %draw.grid 1
  set %draw.gridx 11
  set %draw.gridy 16
  set %draw.delay 3000
  echo $color(info) -si2 --- Variables initialized
  hfree -w gui.*
  hfree -w draw.*
  echo $color(info) -si2 --- Hash tables initialized.
  echo $color(info) -s *** Ascii-Paint has been loaded.  To start: /draw 
}
alias -l draw.getspamchan {
  gui.init @Spam Channel
  window -aBdfhiknop +fnt @Spam Channel -1 -1 320 155
  window -a @Spam Channel
  drawfill -nr @Spam Channel %draw.color2 %draw.color2 1 1
  drawtext -ncr @Spam Channel %Draw.color1 Fixedsys 14 10 10 300 20 Send to (IRC Nickname/Channel):
  gui.edit @Spam Channel 1 10 40 300 20 36 DRAW.SPAM.CHAN %draw.SPAM.CHAN
  gui.set.cmd @Spam Channel 1 draw.spam
  drawtext -ncr @Spam Channel %draw.color1 Fixedsys 14 10 70 300 20 Delay (In milliseconds):
  gui.edit @Spam Channel 2 10 100 300 20 36 DRAW.delay %draw.delay
  gui.set.cmd @Spam Channel 2 draw.spam
  gui.button @Spam Channel 3 10 130 300 20 Spam!
  gui.set.cmd @Spam Channel 3 draw.spam
  gui.forcefocus @Spam Channel 1
}
menu @Spam Channel {
  sclick gui.process.sclick @Spam Channel $mouse.x $mouse.y
}
on 1:keydown:@Spam Channel:*: {
  if ($keyval == 255) { set %draw.gui.keyval.wait 1 | halt }
  if (%draw.gui.keyval.wait == 1) { gui.process.keydown @Spam Channel $keyval ALT $keychar | set %draw.gui.keyval.wait 0 }
  else { 
    gui.process.keydown @Spam Channel $keyval NORMAL $keychar 
  }
}
alias -l draw.fixhash {
  :check
  if ($hget(draw,0).item < %draw.height) {
    hadd -m draw $calc($hget(draw,0).item + 1) $str(0 $+ $chr(44),$calc(%draw.width - 1)) $+ 0
    goto check
  }
  elseif ($hget(draw,0).item > %draw.height) {
    hdel draw $hget(draw,0).item
    goto check
  }
  set %x 1
  while (%x <= %draw.height) {
    if (%draw.width < $numtok($hget(draw,%x),44)) {
      hadd -m draw %x $gettok($hget(draw,%x),1- $+ $calc(%draw.width),44)
    }
    elseif (%draw.width > $numtok($hget(draw,%x),44)) {
      hadd -m draw %x $hget(draw,%x) $+ $str($chr(44) $+ 0,$calc(%draw.width - $numtok($hget(draw,%x),44)))
    }
    else {

    }
    inc %x
  }
}
alias -l draw.toggle.customdim {
  if (%draw.temp.get.usedim == $false) {
    set %draw.temp.get.usedim $true
    gui.button.txt @Receive Image 1 Dont use custom dimensions
  }
  else {
    gui.button.txt @Receive Image 1 Use custom dimensions
    set %draw.temp.get.usedim $false
  }
}

alias rgb-mirc {
  if ($chr(44) !isin $$1) {
    tokenize 44 $rgb($$1)
  }
  else { tokenize 44 $$1 }
  ;echo -s $$1-
  set %x 0
  set %y 765
  set %z 0
  while (%x <= 15) {
    if ($calc($abs($calc($gettok($rgb($color(%x)),1,44) - $$1)) + $abs($calc($gettok($rgb($color(%x)),2,44) - $$2)) + $abs($calc($gettok($rgb($color(%x)),3,44) - $$3))) < %y) {
      set %y $calc($abs($calc($gettok($rgb($color(%x)),1,44) - $$1)) + $abs($calc($gettok($rgb($color(%x)),2,44) - $$2)) + $abs($calc($gettok($rgb($color(%x)),3,44) - $$3)))
      set %z %x
    }
    ;echo -s %x: $calc($abs($calc($gettok($rgb($color(%x)),1,44) - $$1)) + $abs($calc($gettok($rgb($color(%x)),2,44) - $$2)) + $abs($calc($gettok($rgb($color(%x)),3,44) - $$3)))
    inc %x 1
  }
  return %z
}
alias draw.loadimage {
  if ($1) {
    set %draw.loadimage $1-
  }
  else {
    set %draw.loadimage $$sfile($iif(%draw.path,%draw.path,$mircdir),Load image...,Load)
  }
  if ($pic(%draw.loadimage).width = 0) && ($pic(%draw.loadimage).height = 0) {
    ; Invalid file selected, halt loading.
    draw.tip Invalid image file, halted loading.
    halt
  }
  if ($pic(%draw.loadimage).width > 60) || ($pic(%draw.loadimage).height > 30) {
    ; Large image, send warning/confirmation.
    ; Halt for now. --- TEMPORARY
    ; halt
    draw.confirmlargeload
    halt
  }
  draw.loadimage2
}
alias -l draw.loadimage2 {
  if ($window(@Load Image)) { window -c @Load Image }
  draw.tip Loading image...
  window -aBdfhiknop +fnt @li -1 -1 $calc($pic(%draw.loadimage).width + 100) $calc($pic(%draw.loadimage).height + 100)
  drawpic @li 1 1 " $+ %draw.loadimage $+ "
  set -u1 %draw.nodraw 1
  draw.scanimage @li
  set -u1 %draw.nodraw 1
  if ($window(@li)) { window -c @li }
}
alias draw.scanimage {
  set %draw.height $pic(%draw.loadimage).height
  set %draw.width $pic(%draw.loadimage).width
  draw
  set %li.y 1
  set %li.x 1
  hadd -m draw 1 hi
  hfree -w draw
  draw.tip Loading image...
  while (%li.y <= %draw.height) {
    set %li.x 1
    while (%li.x <= %draw.width) {
      if ($len($hget(draw,%li.y)) > 0) {
        if (%li.y == %draw.height) {
        }
        hadd -m draw %li.y $hget(draw,%li.y) $+ $chr(44) $+ $rgb-mirc($rgb($getdot(@li,%li.x,%li.y)))
      }
      else {
        hadd -m draw %li.y $rgb-mirc($rgb($getdot(@li,%li.x,%li.y)))
      }
      inc %li.x
    }
    inc %li.y
  }
  draw.draw
  drawdot @ASCII Paint
  draw.tip Image loaded.
}
alias draw.confirmlargeload {
  if ($window(@Load Image)) { window -c @Load Image }
  gui.init @Load Image
  window -adoBfiknp +fnt @Load Image -1 -1 335 100
  window -a @Load Image
  drawfill -nr @Load Image %draw.color2 %draw.color2 1 1
  drawtext -ncr @Load Image %Draw.color1 Fixedsys 14 1 2 335 20 Warning: Image is large, so unpredictable
  drawtext -ncr @Load Image %draw.color1 Fixedsys 14 1 22 335 20 results may occur. It is not recommended
  drawtext -ncr @Load Image %Draw.color1 Fixedsys 14 1 42 300 20  that you load this image.
  drawline -ncr @Load Image %draw.color1 1 3 67 332 67
  gui.button @Load Image 1 10 72 100 20 Load
  gui.set.cmd @Load Image 1 draw.loadimage2
  gui.button @Load Image 2 225 72 100 20 Don't load
  gui.set.cmd @Load Image 2 window -c @Load Image
}
menu @Load Image {
  sclick gui.process.sclick @Load Image $mouse.x $mouse.y
}
alias draw-reset {  
  if ($window(@ASCII Paint)) { window -c @ASCII Paint }
  if ($window(@Load Image)) { window -c @Load Image }
  if ($window(@Receive Image)) { window -c @Receive Image }
  if ($window(@Spam Channel)) { window -c @Spam Channel }
  if ($window(@Send Image)) { window -c @Send Image }
  if ($window(@Paint Options)) { window -c @Paint Options }
  if ($window(@li)) { window -c @li }
  echo $color(info) -s *** Resetting to ASCII-Paint defaults...
  unset %draw.*
  set %draw.curcolor 1
  set %draw.curcolor2 0
  set %draw.color1 0
  set %draw.color2 $rgb(208,208,208)
  set %draw.height 10
  set %draw.width 30
  set %draw.grid 1
  set %draw.gridx 11
  set %draw.gridy 16
  set %draw.delay 3000
  echo $color(info) -si2 --- Variables reset
  hfree -w gui.*
  hfree -w draw.*
  echo $color(info) -si2 --- Hash tables reset.
  echo $color(info) -s *** Ascii-Paint has been reset to defaults.  To start: /draw 

}

menu menubar, status, channel {
Ascii Paint:/draw
}
