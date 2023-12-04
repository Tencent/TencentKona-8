#!/usr/bin/env bash
#
# Copyright (C) 2022, 2023, THL A29 Limited, a Tencent company. All rights reserved.
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
keyUsage=critical,keyCertSign,cRLSign,digitalSignature
extendedKeyUsage=critical,OCSPSigning
EOF

echo "Generate X.509 version 3 extensions for EE"
cat > ee.ext << EOF
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
EOF

TONGSUO=tongsuo

echo "SM2 key CA, signed by SM3withSM2"
$TONGSUO genpkey -algorithm ec -pkeyopt ec_paramgen_curve:SM2 -pkeyopt ec_param_enc:named_curve -out ca-sm.key
$TONGSUO req -new -key ca-sm.key -subj "/CN=ca-sm" -sm3 -out ca-sm.csr
$TONGSUO x509 -extfile ca.ext -req -CAcreateserial -days 3650 -in ca-sm.csr -sm3 -signkey ca-sm.key -out ca-sm.crt.tmp
$TONGSUO x509 -text -in ca-sm.crt.tmp > ca-sm.crt

echo "SM2 key intermediate CA, signed by SM3withSM2 <- SM2 key CA, signed by SM3withSM2"
$TONGSUO genpkey -algorithm ec -pkeyopt ec_paramgen_curve:SM2 -pkeyopt ec_param_enc:named_curve -out intca-sm.key
$TONGSUO req -new -key intca-sm.key -subj "/CN=intca-sm" -sm3 -out intca-sm.csr
$TONGSUO x509 -extfile ca.ext -req -CAcreateserial -days 3650 -in intca-sm.csr -sm3 \
    -CA ca-sm.crt -CAkey ca-sm.key -out intca-sm.crt.tmp
$TONGSUO x509 -text -in intca-sm.crt.tmp > intca-sm.crt

echo "SM2 key EE, signed SM3withSM2 <- SM2 key intermediate CA, signed SM3withSM2 <- SM2 key CA, signed by SM3withSM2"
$TONGSUO genpkey -algorithm ec -pkeyopt ec_paramgen_curve:SM2 -pkeyopt ec_param_enc:named_curve -out ee-sm.key
$TONGSUO req -new -key ee-sm.key -subj "/CN=ee-sm" -sm3 -out ee-sm.csr
$TONGSUO x509 -extfile ee.ext -req -CAcreateserial -days 3650 -in ee-sm.csr -sm3 \
    -CA intca-sm.crt -CAkey intca-sm.key -out ee-sm.crt.tmp
$TONGSUO x509 -text -in ee-sm.crt.tmp > ee-sm.crt

echo "Alternative SM2 key CA, signed by SM3withSM2"
$TONGSUO genpkey -algorithm ec -pkeyopt ec_paramgen_curve:SM2 -pkeyopt ec_param_enc:named_curve -out ca-sm-alt.key
$TONGSUO req -new -key ca-sm-alt.key -subj "/CN=ca-sm-alt" -sm3 -out ca-sm-alt.csr
$TONGSUO x509 -extfile ca.ext -req -CAcreateserial -days 3650 -in ca-sm-alt.csr -sm3 -signkey ca-sm-alt.key -out ca-sm-alt.crt.tmp
$TONGSUO x509 -text -in ca-sm-alt.crt.tmp > ca-sm-alt.crt

# CRL
echo "CRL: Generate X.509 version 3 extensions for EE"
touch index
cat > tongsuo.cnf << EOF
[crl]
database = index
EOF

$TONGSUO ca -config tongsuo.cnf -name crl -gencrl -cert ca-sm.crt -keyfile ca-sm.key \
    -md sm3 -crldays 3650 -out ca-sm-empty.crl

$TONGSUO ca -config tongsuo.cnf -name crl -cert ca-sm.crt -keyfile ca-sm.key \
    -md sm3 -revoke intca-sm.crt -crl_reason superseded
$TONGSUO ca -config tongsuo.cnf -name crl -gencrl -cert ca-sm.crt -keyfile ca-sm.key \
    -md sm3 -crldays 3650 -out intca-sm.crl

$TONGSUO ca -config tongsuo.cnf -name crl -cert intca-sm.crt -keyfile intca-sm.key \
    -md sm3 -revoke ee-sm.crt -crl_reason superseded
$TONGSUO ca -config tongsuo.cnf -name crl -gencrl -cert intca-sm.crt -keyfile intca-sm.key \
    -md sm3 -crldays 3650 -out ee-sm.crl

# CRL Distribution Points
echo "CRLDP: Generate X.509 version 3 extensions for EE"
cat > ee-crldp.ext << EOF
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
crlDistributionPoints=URI:file:certs/ee-sm-crldp.crl
EOF

echo "CRLDP: SM2 key EE, signed SM3withSM2 <- SM2 key intermediate CA, signed SM3withSM2 <- SM2 key CA, signed by SM3withSM2"
$TONGSUO req -new -key ee-sm.key -subj "/CN=ee-sm-crldp" -sm3 -out ee-sm-crldp.csr
$TONGSUO x509 -extfile ee-crldp.ext -req -CAcreateserial -days 3650 -in ee-sm-crldp.csr -sm3 \
    -CA intca-sm.crt -CAkey intca-sm.key -out ee-sm-crldp.crt.tmp
$TONGSUO x509 -text -in ee-sm-crldp.crt.tmp > ee-sm-crldp.crt

$TONGSUO ca -config tongsuo.cnf -name crl -cert intca-sm.crt -keyfile intca-sm.key \
    -md sm3 -revoke ee-sm-crldp.crt -crl_reason superseded
$TONGSUO ca -config tongsuo.cnf -name crl -gencrl -cert intca-sm.crt -keyfile intca-sm.key \
    -md sm3 -crldays 3650 -out ee-sm-crldp.crl

# AIA
echo "AIA: Generate X.509 version 3 extensions for EE"
cat > ee-aia.ext << EOF
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
authorityInfoAccess=OCSP;URI:http://127.0.0.1:9080
EOF

echo "AIA: SM2 key EE, signed SM3withSM2 <- SM2 key intermediate CA, signed SM3withSM2 <- SM2 key CA, signed by SM3withSM2"
$TONGSUO req -new -key ee-sm.key -subj "/CN=ee-sm-aia" -sm3 -out ee-sm-aia.csr
$TONGSUO x509 -extfile ee-aia.ext -req -CAcreateserial -days 3650 -in ee-sm-aia.csr -sm3 \
    -CA intca-sm.crt -CAkey intca-sm.key -out ee-sm-aia.crt.tmp
$TONGSUO x509 -text -in ee-sm-aia.crt.tmp > ee-sm-aia.crt

rm -f *.csr *.tmp *.srl *.enc
