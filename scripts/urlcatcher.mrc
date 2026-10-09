on *:text:*.exe*:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.exe*,1,32) }
on *:text:*.rar*:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.rar*,1,32) }
on *:text:*.zip*:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.zip*,1,32) }
on *:text:*.tar*:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.tar*,1,32) }
on *:text:*.jpg*:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.jpg*,1,32) }
on *:text:*.gif*:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.gif*,1,32) }
on *:text:*.bmp*:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.bmp*,1,32) }
on *:text:*.png*:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.png*,1,32) }
on *:text:*.tgz*:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.tgz*,1,32) }
on *:text:*.gz*:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.tgz*,1,32) }
on *:text:*.c:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.c,1,32) }
on *:text:*.cpp:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.cpp,1,32) }
on *:text:*.htm:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.htm,1,32) }
on *:text:*.html:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.html,1,32) }
on *:text:*.php:*:{ .signal url $nick $ctime $iif($chan,$chan,Private) $network $wildtok($strip($1-), *.php,1,32) }




on *:signal:url:{

  if (http isin $1-) || (https isin $1-) || (ftp isin $1-) || (www isin $1-) || (.net isin $1-) || (.com isin $1-) || (.org isin $1-)  {

    if (!$lines("urls.html")) {
      write "urls.html" <html><head><title> Url Log </title></head><body><font face=fixedsys> $+ $str($lf,3) $+ </body></html>
    }
    write "urls.html" $asctime(mm-dd-yyyy) $asctime(HH:nn:ss) $3 $1 <A HREF=" $+ $5 $+ "> $+ $5 $+ </a><br> $lf

  }

}
