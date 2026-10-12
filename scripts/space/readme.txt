I changed all bmp to png.  
much better transparency!     
ELY M.  (ELY3M on github)


IRC Invaders v1.1 - by entropy 2020 (8/18/20)

================================================

Installation:
First, unzip "space.zip" to the root mirc 
directory. Not in mirc\scripts, but mirc\.
Then: /load -rs space\space.mrc

================================================

Welcome to my Space Game! I have spent a good week
making this as nice and interesting as possible.
Also, a lot of time squashing bugs :)

So far the Single Player is about done. Next,
I work on multiplayer.

I might want to add support for items and other
weapons at some point.

================================================

This ZIP comes with some TTF fonts. Double click 
and install them before playing.

================================================

The controls:
[←] Left
[→] Right
[↑] Up
[↓] Down

Other buttons:
[ctrl]  Fire Button
[F1]    Top 10 Scores
[PAUSE] Pause the game

================================================

WARNING: Players (in the levels ini, under: 
[waveN] with the exact same X/Y pixels will 
crash the game!

================================================

For maps/levels, this game works with external 
INI files. Each one, is labled level1.ini, 
level2.ini etc.

You must have them ordered by levelN.ini

================================================

To customize enemies on levels, you must edit 
the corisponding INI file.

Enemies, INI section is:

[wave1]
n0=stuff here

"wave1" is the first "wave" of enemies.
so to have multiple waves, you must specify 
the [waveN] and add items starting from "n0", 
and increasing to "n1" etc.

================================================

INI File declaration of game enemies:

[wave1]
n0=new_player1 100 -300 ball.bmp 200 128 128 down 50 2 new_player1 1 1100 2000

================================================

Definition: 
<n0> <player> <x> <y> <image> <health> <w> <h> <angle> <damage> <speed> <owner> <0=straight movement,1=back and forth movement,2=no movement> <shot (milliseconds)> <points when dead>

All players are to be "new_playerN" with increasing 
player name per enemy. Owner can be your player name. 
Only important for missles.

================================================

Next, is the setting for each level. Example:

[settings]
health=100                /* Health when starting level */
lives=2                   /* Starting lives */
speed=8                   /* Speed of movement for your player */
shot=800                  /* Milliseconds between shots for your player */
remainder=100             /* Your health remainder */
width=48                  /* Width in pixels of your player */
height=48                 /* Height in pixels of your player */
damage=25                 /* Damage per shot colliding with enemy */
owned=player              /* Not important */
pershot=50                /* Points per successfully landed shot (not if missles collide) */
misslespeed=10            /* Missle travel speed (do NOT make this larger than 10 */
protect=3                 /* Protection seconds when respawning */
background=space\bg.jpg   /* The background image of hte level */
hud=47                    /* HUD color (must be mirc's 99 colors, so: 00-99) */
clear=5000                /* Points awarded to the player for beating this level */

================================================

warning:
you need to specify at least -150 (Y) difference when adding entities.

it's good to do (y):
-200
-350
-500
-650

etc..

================================================