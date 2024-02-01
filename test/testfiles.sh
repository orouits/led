#!/bin/bash

mkdir -p test/files

cat - > test/files/file_1<<EOT
sdvmiksfqs
TEST 11111111
SLFKSLDkfj
111111 TEST SDFMLSDF
dfsldkfjsldf
EOT

cat - > test/files/file_2<<EOT
2222222é
TEST 22222222
SLFKSLDkfj
222222 TEST SDFMLSDF
dfsldkfjsldf
EOT


# ls test/files/* | led -v TEST -f

# ls test/files/* | led -v TEST -F -f

# ls test/files/* | led -v TEST -Wtest/files/write -f

# ls test/files/* | led -v TEST -Atest/files/append -f

ls test/files/* | led -v TEST -Dtest/files_out -f

cat test/files/*