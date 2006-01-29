# Script to load a prx and its symbols, set a breakpoint on its main
# function and start it.
# Call it 'run filename modulename'
modload $1.prx
symload $1.sym
bpset ?$2:main?
modstart @$2 $1.prx
