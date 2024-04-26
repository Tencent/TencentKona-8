#!/usr/bin/env bash
#
# Copyright (C) 2022, 2024, THL A29 Limited, a Tencent company. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#

echo "Generate X.509 version 3 extensions for CA"
cat > ca.ext << EOF
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
basicConstraints=critical,CA:TRUE
keyUsage=critical,digitalSignature,keyCertSign,cRLSign
EOF

echo "Generate X.509 version 3 extensions for sign EE"
cat > ee-sign.ext << EOF
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature
EOF

echo "Generate X.509 version 3 extensions for enc EE"
cat > ee-enc.ext << EOF
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
basicConstraints=critical,CA:FALSE
keyUsage=critical,keyEncipherment,dataEncipherment,keyAgreement
EOF

OPENSSL=tongsuo

##### CA
$OPENSSL genpkey -algorithm ec -pkeyopt ec_paramgen_curve:SM2 -pkeyopt ec_param_enc:named_curve -out tlcp-ca.key
$OPENSSL req -new -key tlcp-ca.key -subj "/CN=tlcp-ca" -sm3 -out tlcp-ca.csr
$OPENSSL x509 -extfile ca.ext -req -CAcreateserial -days 3650 -in tlcp-ca.csr -sm3 \
    -signkey tlcp-ca.key -out tlcp-ca.crt.tmp
$OPENSSL x509 -text -in tlcp-ca.crt.tmp > tlcp-ca.crt

##### Intermediate CA
$OPENSSL genpkey -algorithm ec -pkeyopt ec_paramgen_curve:SM2 -pkeyopt ec_param_enc:named_curve -out tlcp-intca.key
$OPENSSL req -new -key tlcp-intca.key -subj "/CN=tlcp-intca" -sm3 -out tlcp-intca.csr
$OPENSSL x509 -extfile ca.ext -req -CAcreateserial -days 3650 -in tlcp-intca.csr -sm3 \
    -CA tlcp-ca.crt -CAkey tlcp-ca.key -out tlcp-intca.crt.tmp
$OPENSSL x509 -text -in tlcp-intca.crt.tmp > tlcp-intca.crt

##### Sign EE (Server)
$OPENSSL genpkey -algorithm ec -pkeyopt ec_paramgen_curve:SM2 -pkeyopt ec_param_enc:named_curve -out tlcp-server-sign.key
$OPENSSL req -new -key tlcp-server-sign.key -subj "/CN=tlcp-server-sign" -sm3 -out tlcp-server-sign.csr
$OPENSSL x509 -extfile ee-sign.ext -req -CAcreateserial -days 3650 -in tlcp-server-sign.csr -sm3 \
    -CA tlcp-intca.crt -CAkey tlcp-intca.key -out tlcp-server-sign.crt.tmp
$OPENSSL x509 -text -in tlcp-server-sign.crt.tmp > tlcp-server-sign.crt

##### Sign EE (Client)
$OPENSSL genpkey -algorithm ec -pkeyopt ec_paramgen_curve:SM2 -pkeyopt ec_param_enc:named_curve -out tlcp-client-sign.key
$OPENSSL req -new -key tlcp-client-sign.key -subj "/CN=tlcp-client-sign" -sm3 -out tlcp-client-sign.csr
$OPENSSL x509 -extfile ee-sign.ext -req -CAcreateserial -days 3650 -in tlcp-client-sign.csr -sm3 \
    -CA tlcp-intca.crt -CAkey tlcp-intca.key -out tlcp-client-sign.crt.tmp
$OPENSSL x509 -text -in tlcp-client-sign.crt.tmp > tlcp-client-sign.crt

##### Enc EE (Server)
$OPENSSL genpkey -algorithm ec -pkeyopt ec_paramgen_curve:SM2 -pkeyopt ec_param_enc:named_curve -out tlcp-server-enc.key
$OPENSSL req -new -key tlcp-server-enc.key -subj "/CN=tlcp-server-enc" -sm3 -out tlcp-server-enc.csr
$OPENSSL x509 -extfile ee-enc.ext -req -CAcreateserial -days 3650 -in tlcp-server-enc.csr -sm3 \
    -CA tlcp-intca.crt -CAkey tlcp-intca.key -out tlcp-server-enc.crt.tmp
$OPENSSL x509 -text -in tlcp-server-enc.crt.tmp > tlcp-server-enc.crt

##### Enc EE (Client)
$OPENSSL genpkey -algorithm ec -pkeyopt ec_paramgen_curve:SM2 -pkeyopt ec_param_enc:named_curve -out tlcp-client-enc.key
$OPENSSL req -new -key tlcp-client-enc.key -subj "/CN=tlcp-client-enc" -sm3 -out tlcp-client-enc.csr
$OPENSSL x509 -extfile ee-enc.ext -req -CAcreateserial -days 3650 -in tlcp-client-enc.csr -sm3 \
    -CA tlcp-intca.crt -CAkey tlcp-intca.key -out tlcp-client-enc.crt.tmp
$OPENSSL x509 -text -in tlcp-client-enc.crt.tmp > tlcp-client-enc.crt

rm *.tmp *.csr
