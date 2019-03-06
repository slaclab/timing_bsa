# mpsplot.p

set multiplot layout 1,2
plot "$0" using 1:3  title 'valid'
plot "$0" using 1:6  title 'PC0'
unset multiplot
