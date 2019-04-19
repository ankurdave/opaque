set pagination off
handle SIGSEGV pass
catch signal SIGSEGV
commands
bt
c
end
r -Xmx4g org.scalatest.run edu.berkeley.cs.rise.opaque.QEDSuite
bt
