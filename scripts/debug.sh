# Script to load a prx and its symbols, set a breakpoint on its main
# function and start it.
# Call it 'run filename'
modload $1.prx
symload $1.sym
bpset ?$!:main?
modstart @$! $1.prx
