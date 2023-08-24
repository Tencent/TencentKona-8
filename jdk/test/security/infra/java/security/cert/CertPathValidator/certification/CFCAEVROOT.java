/*
 * Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * @test
 * @summary Interoperability tests with CFCA EV ROOT CA from CFCA
 * @build ValidatePathWithParams
 * @run main/othervm -Djava.security.debug=certpath CFCAEVROOT OCSP
 * @run main/othervm -Djava.security.debug=certpath CFCAEVROOT CRL
 */

/*
 * Obtain TLS test artifacts for CFCA EV ROOT CA from:
 *
 * Valid TLS Certificate:
 * https://www.anxinsign.com
 *
 * Expired TLS Certificate:
 * https://exp.cebnet.com.cn
 *
 * Revoked TLS Certificate:
 * https://rev.cebnet.com.cn
 */
public class CFCAEVROOT {

    // Owner: C = CN, O = China Financial Certification Authority, CN = CFCA EV OCA
    // Issuer: C = CN, O = China Financial Certification Authority, CN = CFCA EV ROOT
    // Serial number: 0xb4cf943266
    // Valid from: Aug 8 06:06:31 2012 GMT until: Dec 29 06:06:31 2029 GMT
    private static final String CFCA_EV_OCA =
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIFTjCCAzagAwIBAgIGALTPlDJmMA0GCSqGSIb3DQEBCwUAMFYxCzAJBgNVBAYT\n" +
            "AkNOMTAwLgYDVQQKDCdDaGluYSBGaW5hbmNpYWwgQ2VydGlmaWNhdGlvbiBBdXRo\n" +
            "b3JpdHkxFTATBgNVBAMMDENGQ0EgRVYgUk9PVDAeFw0xMjA4MDgwNjA2MzFaFw0y\n" +
            "OTEyMjkwNjA2MzFaMFUxCzAJBgNVBAYTAkNOMTAwLgYDVQQKDCdDaGluYSBGaW5h\n" +
            "bmNpYWwgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkxFDASBgNVBAMMC0NGQ0EgRVYg\n" +
            "T0NBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA02OMsGFxFQIPMKVP\n" +
            "oRaO9rHNX41xbq8jhnbdK0MDVbxfGa3b8QTKxMcmxlRlULfsaie0cIlaRl0AUcJP\n" +
            "QH9ftekzh4T287xqsEAydYQHf77arWQ5nY3fR9RcoBq9pTCQbqw49S6/jHA5oPQa\n" +
            "EoKbF0G8zfVKp5PrcKSufHMQyKo/Ez2UYT+gut36j4GYpAABuV6PbusPpjufsN9B\n" +
            "r9+xqgyz8ubSp1Wl1qSlvQUQBhAJAH+a3NMhD0illaGfTdWbF485a5NilMFGqJBa\n" +
            "/kLVEYwG4aoKdV9vG/NFS0LKz3QVnB7bkrLjTkuGN/zQJP0daJ3CGAzmN+Cr2ujt\n" +
            "XOfAYwIDAQABo4IBITCCAR0wOAYIKwYBBQUHAQEELDAqMCgGCCsGAQUFBzABhhxo\n" +
            "dHRwOi8vb2NzcC5jZmNhLmNvbS5jbi9vY3NwMB8GA1UdIwQYMBaAFOP+Lf0o0Au1\n" +
            "uraixL8GqgWMk/svMA8GA1UdEwEB/wQFMAMBAf8wRAYDVR0gBD0wOzA5BgRVHSAA\n" +
            "MDEwLwYIKwYBBQUHAgEWI2h0dHA6Ly93d3cuY2ZjYS5jb20uY24vdXMvdXMtMTIu\n" +
            "aHRtMDoGA1UdHwQzMDEwL6AtoCuGKWh0dHA6Ly9jcmwuY2ZjYS5jb20uY24vZXZy\n" +
            "Y2EvUlNBL2NybDEuY3JsMA4GA1UdDwEB/wQEAwIBBjAdBgNVHQ4EFgQUVQji3MyV\n" +
            "bR9d3rNH6OkWxsBFd8QwDQYJKoZIhvcNAQELBQADggIBAMmFEIoCE9UNmb2BYYhT\n" +
            "RV12kNVucP6t683BaFTgJizIJw/ebvvTdWNTycyP5MQFlHKrIYwjvFO9Rfw8+yIs\n" +
            "sT3JFYiqsLBswvaMr3AIuA2mTnmasvZFe6P19qitzTRkz+TL6TFailrtnzudsvn2\n" +
            "SeVbRiX+6CsyNNMoPsRHTeZAEpkB7J3vh+ZAiv3gsIXtjtz5Y1iWWRZipemJ/qEf\n" +
            "W2hDONB+T6lGcEXHDi9dIkWcC/jFT4XPM64pagAz9gEGZg1PzFBE8QMxiwaDAOea\n" +
            "G0l0e/HW4wJlo4ZzOELqZGJLlYhQ8AkBYR95NEtR9j5bWK98Lznykldk2MDLBD2m\n" +
            "rIfMkVjMwEj4A8ElMXsLnWXXg41NN6gjUm2/IudKOaGqniPs5SZrN36O4B3NzsaZ\n" +
            "dLznHH5H0+aksurjgme8RAG0A2OAnRG3VXBWrxud7t0KDINLs+mxY7IR+xVZ2cw6\n" +
            "Cer8HnAVfKPJrbdq7vyJJkIpCll+mLHaGgvv3IqiU4rrrllE3NYjKG4Fk2MiYvZg\n" +
            "10KXA8tlYsLt8I/RcNmC2TvjZHYVE3tanbGw53TRGFk2Vq68XOkvooOardihwRkg\n" +
            "qcOgUvouORuvSqTlkQizTFH6FTUt3xuuED4dnn5N/1ijcDt0N3l5ovoyHOVcYiO4\n" +
            "drCN96LHiUoiSfYODmpXG2tl\n" +
            "-----END CERTIFICATE-----";

    // Owner: C = CN, O = China Financial Certification Authority, CN = CFCA OV OCA
    // Issuer: C = CN, O = China Financial Certification Authority, CN = CFCA EV ROOT
    // Serial number: 0xf9df6adff564bea68b82
    // Valid from: Mar 25 02:02:56 2015 GMT until: Dec 25 02:02:56 2029 GMT
    private static final String CFCA_OV_OCA =
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIFfDCCA2SgAwIBAgILAPnfat/1ZL6mi4IwDQYJKoZIhvcNAQELBQAwVjELMAkG\n" +
            "A1UEBhMCQ04xMDAuBgNVBAoMJ0NoaW5hIEZpbmFuY2lhbCBDZXJ0aWZpY2F0aW9u\n" +
            "IEF1dGhvcml0eTEVMBMGA1UEAwwMQ0ZDQSBFViBST09UMB4XDTE1MDMyNTAyMDI1\n" +
            "NloXDTI5MTIyNTAyMDI1NlowVTELMAkGA1UEBhMCQ04xMDAuBgNVBAoMJ0NoaW5h\n" +
            "IEZpbmFuY2lhbCBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eTEUMBIGA1UEAwwLQ0ZD\n" +
            "QSBPViBPQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDn14xTy0bH\n" +
            "zkaeyACeq6ryfxxG5zZT1fCL4lmw7sk6SVmOKNfE60Gf7W6orksrFVIbIMK+VrYp\n" +
            "+aYyhScq8EJT9xXBgXK2HqtpaDGOeclspJvcs+rXn9tlT789NBp3i5U+nLE9M1bR\n" +
            "CHSx3Hzu8p7Aeqllou+8nZ2egaVbWFL1zC1JENupSSI9Yjbefhb06y/TVxQ0x4Zt\n" +
            "zwPwLcd8NUtSruldolxPbhQeCZNJMPq1GKMxhd5pDwY4mCKxDeraqhTNXui9Aef3\n" +
            "qyi2Ic9EXmdNPARkZJU2XTJ9FJ+DE+ChaIvfJ/VwQfM0eGlBn/SAaav54jBmRnec\n" +
            "PeD6YfpuiJ8vAgMBAAGjggFKMIIBRjA4BggrBgEFBQcBAQQsMCowKAYIKwYBBQUH\n" +
            "MAGGHGh0dHA6Ly9vY3NwLmNmY2EuY29tLmNuL29jc3AwHwYDVR0jBBgwFoAU4/4t\n" +
            "/SjQC7W6tqLEvwaqBYyT+y8wDwYDVR0TAQH/BAUwAwEB/zBEBgNVHSAEPTA7MDkG\n" +
            "BFUdIAAwMTAvBggrBgEFBQcCARYjaHR0cDovL3d3dy5jZmNhLmNvbS5jbi91cy91\n" +
            "cy0xMi5odG0wOgYDVR0fBDMwMTAvoC2gK4YpaHR0cDovL2NybC5jZmNhLmNvbS5j\n" +
            "bi9ldnJjYS9SU0EvY3JsMS5jcmwwDgYDVR0PAQH/BAQDAgEGMB0GA1UdDgQWBBRm\n" +
            "s+/7VJWH6aylllau5n3tOtBD0TAnBgNVHSUEIDAeBggrBgEFBQcDAgYIKwYBBQUH\n" +
            "AwQGCCsGAQUFBwMBMA0GCSqGSIb3DQEBCwUAA4ICAQDKER8qcBmZGOG8GOJ670VW\n" +
            "OSg3UovOoc7/xz2mE+enyEcSwn/0QrL8C5DSA6nMvBMrCWEytYPofGQUXTwtlu78\n" +
            "GLxYNn3A/RtzczJ+/BXIhoe3aOT4tQ+2s9vrFRfIXs4CkmqHhfYSvArokdayYmBd\n" +
            "78psIwS5LCUzGKSn7y8UmAgoxiy7RtrVt5c1wvJyeuYk1Z1l8MN1szPrmAb4HS/D\n" +
            "qnB+0qdhFGvfOyv7lg6/wlIAkN84cHlKNC3JvyFHaCIAyhTPgjUayUvBKFK7XwN9\n" +
            "utIXl2L3IZX7zfxGS/J9+ZeNwyblQKmd/MKydJu9Ak6+ZMLLgjlCFkihJIn9Ur8M\n" +
            "2KQigz7YPDVIJjOtS7ljOQVGh88LPUnQ1fBY7RwagficS/xclIOnaXhoyWzg7EcQ\n" +
            "/T1/O4FkpMqKuOreaI5NExjAT8cKizyY2wcOOXKIYri3Ewnbm+00IaYYaiQRGUR6\n" +
            "pzFFKxdFMbStCtI4ObN+A9tB7cnBCW4vz3sAJd/OgmLF38XTa+/km3c1nQOfhCGs\n" +
            "6kx2heN/DgFAc+P7ldObo/kgGQtR6tr02gyXCFWnLMtT0+CoNOY0o3T+LbEqYeKL\n" +
            "W7p29G9sgHgoqLFibWNMSKG1QvevkhjUMOD/g48f/nMSYsbU++yEaLvjvRHbb5ON\n" +
            "IPkcE28TRhQQKmDKI+DRIg==\n" +
            "-----END CERTIFICATE-----";

    // Owner: jurisdictionC = CN, jurisdictionST = Beijing, jurisdictionL = Beijing,
    // businessCategory = Private Organization, serialNumber = 110102015818793,
    // C = CN, ST = Beijing, L = Beijing, postalCode = 100176,
    // street = 北京市亦庄经济技术开发区科创14街20号院2号楼, CN = www.anxinsign.com
    // Issuer: C = CN, O = China Financial Certification Authority, CN = CFCA EV OCA
    // Serial number: 0x758f4b981578a830de8e
    // Valid from: Oct 13 06:47:00 2022 GMT until: Nov 11 03:35:50 2023 GMT
    private static final String VALID =
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIHRDCCBiygAwIBAgIKdY9LmBV4qDDejjANBgkqhkiG9w0BAQsFADBVMQswCQYD\n" +
            "VQQGEwJDTjEwMC4GA1UECgwnQ2hpbmEgRmluYW5jaWFsIENlcnRpZmljYXRpb24g\n" +
            "QXV0aG9yaXR5MRQwEgYDVQQDDAtDRkNBIEVWIE9DQTAeFw0yMjEwMTMwNjQ3MDBa\n" +
            "Fw0yMzExMTEwMzM1NTBaMIIBWDETMBEGCysGAQQBgjc8AgEDEwJDTjEYMBYGCysG\n" +
            "AQQBgjc8AgECDAdCZWlqaW5nMRgwFgYLKwYBBAGCNzwCAQEMB0JlaWppbmcxHTAb\n" +
            "BgNVBA8MFFByaXZhdGUgT3JnYW5pemF0aW9uMRgwFgYDVQQFEw8xMTAxMDIwMTU4\n" +
            "MTg3OTMxCzAJBgNVBAYTAkNOMRAwDgYDVQQIDAdCZWlqaW5nMRAwDgYDVQQHDAdC\n" +
            "ZWlqaW5nMQ8wDQYDVQQRDAYxMDAxNzYxRzBFBgNVBAkMPuWMl+S6rOW4guS6puW6\n" +
            "hOe7j+a1juaKgOacr+W8gOWPkeWMuuenkeWImzE06KGXMjDlj7fpmaIy5Y+35qW8\n" +
            "MS0wKwYDVQQKDCTljJfkuqzkuK3ph5Hlm73kv6Hnp5HmioDmnInpmZDlhazlj7gx\n" +
            "GjAYBgNVBAMMEXd3dy5hbnhpbnNpZ24uY29tMIIBIjANBgkqhkiG9w0BAQEFAAOC\n" +
            "AQ8AMIIBCgKCAQEAvqvVct1ibFzY1prZRgRKIp6+F8BTx6AAc1rKdXJlqyzaziO5\n" +
            "NMXquFySddUFlTC2qllSKzk7rSC0Rb/17UySPbvIHYb5F6CRHA5c00rxcB3Nd4FR\n" +
            "YQ4ux2jga3G1olsiUZSK8qsePRcjRgR0QGThZ24F5PO2a82lc242qr5SGuG6HfUZ\n" +
            "3zFyW838oM2witK/faMllKkL7r87uQ3PfOrfHT8zGDQA5c1x58LZeVLMIaFRrcOm\n" +
            "v//dyCqU/JR+/atYB94b9jRGTSGXe3APYC/krKBbGcBM3MlDdHepge7oNTVTfRI3\n" +
            "vjGeziVdu7xLfo2px9q9ISpTey+HuDdQDEiLGQIDAQABo4IDDzCCAwswCQYDVR0T\n" +
            "BAIwADBsBggrBgEFBQcBAQRgMF4wKAYIKwYBBQUHMAGGHGh0dHA6Ly9vY3NwLmNm\n" +
            "Y2EuY29tLmNuL29jc3AwMgYIKwYBBQUHMAKGJmh0dHA6Ly9ndGMuY2ZjYS5jb20u\n" +
            "Y24vZXZvY2EvZXZvY2EuY2VyMBwGA1UdEQQVMBOCEXd3dy5hbnhpbnNpZ24uY29t\n" +
            "MAsGA1UdDwQEAwIFoDAdBgNVHQ4EFgQUSE7l9QtYG2Wj5D5U7U7O1Bw+GHkwEwYD\n" +
            "VR0lBAwwCgYIKwYBBQUHAwEwggF+BgorBgEEAdZ5AgQCBIIBbgSCAWoBaAB2AOg+\n" +
            "0No+9QY1MudXKLyJa8kD08vREWvs62nhd31tBr1uAAABg9AYsAEAAAQDAEcwRQIg\n" +
            "RqA54KScSxpiymSO4vh6GdxvGmadt8kyD3cLPbnh5LUCIQDRN+d74tryNYbGhg43\n" +
            "jknoQY6gmHBHssr0j2XchIpWqAB2AK33vvp8/xDIi509nB4+GGq0Zyldz7EMJMqF\n" +
            "hjTr3IKKAAABg9AYvAgAAAQDAEcwRQIgLq9gTiSLMV+lufUzVWNHgypZEH0k2t+K\n" +
            "d3IWDtrCp7UCIQD5bUJo3iJXg535PLFQCytICSr3rJ0paM8bvX9ELwMOawB2AG9T\n" +
            "dqwx8DEZ2JkApFEV/3cVHBHZAsEAKQaNsgiaN9kTAAABg9AYwOoAAAQDAEcwRQIh\n" +
            "AI63qBg2G9M94kvGraObA7Kb7I1yUVi3zt8HWcpv/RSHAiAnscXLRZ3kT9p6mjWq\n" +
            "VybfdFDIhNUbrb/UnjSlbNcSEzAfBgNVHSMEGDAWgBRVCOLczJVtH13es0fo6RbG\n" +
            "wEV3xDBQBgNVHSAESTBHMDwGB2CBHIbvKgMwMTAvBggrBgEFBQcCARYjaHR0cDov\n" +
            "L3d3dy5jZmNhLmNvbS5jbi91cy91cy0xMi5odG0wBwYFZ4EMAQEwPAYDVR0fBDUw\n" +
            "MzAxoC+gLYYraHR0cDovL2NybC5jZmNhLmNvbS5jbi9ldm9jYS9SU0EvY3JsMTYx\n" +
            "LmNybDANBgkqhkiG9w0BAQsFAAOCAQEACSPvvisunQrYZhx7rU4eeeWXFmQLszxT\n" +
            "lYibEXZi9G19GA1Jj4qujzba/KufvU3zd+9RhA1MuwujcxCY5XwB2xsRu2VGVi9r\n" +
            "FmYDMxbbbowAcphyAvupvpo5bA6N+CcIOQULCMIMcXdGSdVFou/D0E/iIvtbkCaq\n" +
            "EfqJwWGHEiqr1VdKNjBKtfxMPDR9QgURJbYwApDMS2SqU/cGSZ56gwXOmyeVWTHh\n" +
            "/pJ8wH1GjMjMh7NwWOUPkK+mqR+hd1hlQJBFFkvhnZTROLQP4aDsuq9F9JSIfSI4\n" +
            "hIG4uoG6bWBoaChxcN4q+n9Xapr3n8SJI34tFa3GenhrzrrQhICX/A==\n" +
            "-----END CERTIFICATE-----";

    // Owner: C = CN, jurisdictionC = CN, jurisdictionST = Beijing, jurisdictionL = Beijing,
    // businessCategory = Private Organization, serialNumber = 110000006499259,
    // ST = Beijing, L = Beijing, O = China Financial Certification Authority,
    // OU = E-Banking Network, CN = exp.cebnet.com.cn
    // Issuer: C = CN, O = China Financial Certification Authority, CN = CFCA OV OCA
    // Serial number: 0x201501f58c5fe24387538a5e6325e492
    // Valid from: Nov 2 08:52:47 2022 GMT until: Nov 2 08:52:47 2023 GMT
    private static final String EXPIRED =
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIFfzCCBGegAwIBAgIKbZ42vcKMk+f0zjANBgkqhkiG9w0BAQsFADBVMQswCQYD\n" +
            "VQQGEwJDTjEwMC4GA1UECgwnQ2hpbmEgRmluYW5jaWFsIENlcnRpZmljYXRpb24g\n" +
            "QXV0aG9yaXR5MRQwEgYDVQQDDAtDRkNBIEVWIE9DQTAeFw0xNDAzMTMwNjAzMTla\n" +
            "Fw0xNDA0MTMwNjAzMTlaMIIBHTELMAkGA1UEBhMCQ04xEzARBgsrBgEEAYI3PAIB\n" +
            "AwwCQ04xGDAWBgsrBgEEAYI3PAIBAgwHQmVpamluZzEYMBYGCysGAQQBgjc8AgEB\n" +
            "DAdCZWlqaW5nMR0wGwYDVQQPDBRQcml2YXRlIE9yZ2FuaXphdGlvbjEYMBYGA1UE\n" +
            "BQwPMTEwMDAwMDA2NDk5MjU5MRAwDgYDVQQIDAdCZWlqaW5nMRAwDgYDVQQHDAdC\n" +
            "ZWlqaW5nMTAwLgYDVQQKDCdDaGluYSBGaW5hbmNpYWwgQ2VydGlmaWNhdGlvbiBB\n" +
            "dXRob3JpdHkxGjAYBgNVBAsMEUUtQmFua2luZyBOZXR3b3JrMRowGAYDVQQDDBFl\n" +
            "eHAuY2VibmV0LmNvbS5jbjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n" +
            "AJYxKFn2IQ18DSvu9+76ZA9dq5Zha2KYgBykjOBiFFvCLf/03k6xE/b3teCKqjtl\n" +
            "xfKaLjdyYL2e3Hi9B2nokrGe5urNPNY176bclirHFUz/tHIWEQM9LNk7+gw/eTaJ\n" +
            "UOnqoGW5T9sfJZN6fQl6AY6h2rdBtiXdpNMxS24rTRcLsblaBmVQ1CTM2tzOHj2r\n" +
            "iS5mE7YH9H9Bw0RVM5FeFod0DPsjYSvP9JzY6530Z1ABtCdIcM2astXu9hKiCK7U\n" +
            "8Dqk6uIkAwvEGbLkDPoIa3cUC5KfgfP3oVTmTIYSTwMNxe6dYTPhqbOynswno4U0\n" +
            "jK5uITCP764WaCVgZlS1NykCAwEAAaOCAYUwggGBMAwGA1UdEwQFMAMBAQAwbAYI\n" +
            "KwYBBQUHAQEEYDBeMCgGCCsGAQUFBzABhhxodHRwOi8vb2NzcC5jZmNhLmNvbS5j\n" +
            "bi9vY3NwMDIGCCsGAQUFBzAChiZodHRwOi8vZ3RjLmNmY2EuY29tLmNuL2V2b2Nh\n" +
            "L2V2b2NhLmNlcjAcBgNVHREEFTATghFleHAuY2VibmV0LmNvbS5jbjALBgNVHQ8E\n" +
            "BAMCBaAwHQYDVR0OBBYEFHjW8XRg66TUS2aUVAyIvg3iw28LMBMGA1UdJQQMMAoG\n" +
            "CCsGAQUFBwMBMB8GA1UdIwQYMBaAFFUI4tzMlW0fXd6zR+jpFsbARXfEMEcGA1Ud\n" +
            "IARAMD4wPAYHYIEchu8qAzAxMC8GCCsGAQUFBwIBFiNodHRwOi8vd3d3LmNmY2Eu\n" +
            "Y29tLmNuL3VzL3VzLTEyLmh0bTA6BgNVHR8EMzAxMC+gLaArhilodHRwOi8vY3Js\n" +
            "LmNmY2EuY29tLmNuL2V2b2NhL1JTQS9jcmwxLmNybDANBgkqhkiG9w0BAQsFAAOC\n" +
            "AQEAMrt5ghB/SZF+nckes2GJn8L7W4t3lR27DWqc2O9R0SffkTBgi3f4w+uisXOp\n" +
            "41EsvaAN9AYIXfF+Cg4tyGuuBXe05i4CrGP4cOGX4FqaF+55tT8d8RrwL1pgyhCS\n" +
            "WLJJsldPiLQ1UnTZ834Gjz/cfykVmtajJtx1EmgMKGpDFcp72ttbj4JePtJX7acc\n" +
            "7r5hn6/SvtSh+bnttsWDUI2s3vzj99DBn540mzDIMhVFqxmS/LVa8bhKEcsxiw6r\n" +
            "XSa8ooA6psPTplLVDrSHEHp5Co5GcmfpIo0011rmHwT8nrL/SPPiJKCJ0KrZyKsI\n" +
            "eobxpX4juyXHxPxlVb4LwEJgVQ==\n" +
            "-----END CERTIFICATE-----";

    // Owner: C = CN, ST = 北京, L = 北京, O = 中金金融认证中心有限公司, CN = rev.cebnet.com.cn
    // Issuer: C = CN, O = China Financial Certification Authority, CN = CFCA OV OCA
    // Serial number: 0x201501f58c5fe24387538a5e6325e492
    // Valid from: Nov 2 08:52:47 2022 GMT until: Nov 2 08:52:47 2023 GMT
    private static final String REVOKED =
            "-----BEGIN CERTIFICATE-----\n" +
            "MIIGbjCCBVagAwIBAgIQIBUB9Yxf4kOHU4peYyXkkjANBgkqhkiG9w0BAQsFADBV\n" +
            "MQswCQYDVQQGEwJDTjEwMC4GA1UECgwnQ2hpbmEgRmluYW5jaWFsIENlcnRpZmlj\n" +
            "YXRpb24gQXV0aG9yaXR5MRQwEgYDVQQDDAtDRkNBIE9WIE9DQTAeFw0yMjExMDIw\n" +
            "ODUyNDdaFw0yMzExMDIwODUyNDdaMHoxCzAJBgNVBAYTAkNOMQ8wDQYDVQQIDAbl\n" +
            "jJfkuqwxDzANBgNVBAcMBuWMl+S6rDEtMCsGA1UECgwk5Lit6YeR6YeR6J6N6K6k\n" +
            "6K+B5Lit5b+D5pyJ6ZmQ5YWs5Y+4MRowGAYDVQQDDBFyZXYuY2VibmV0LmNvbS5j\n" +
            "bjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKNzDSGOBR7O6cafH9pt\n" +
            "te3wXYDpZJzrFH/fvKUSs9ReBh+DejaqsY4Eg+1+RyvlqT5eWphJ8E/RF33h5Faw\n" +
            "5OLSeTh1TTty3QaM5JGH5wRBamVHQ1OesGGgOFZHUsM+pPuG3eD8dgDBw55QjHlK\n" +
            "ah7nZU7fKmRLbxrb9x+w8gdi3+4tjXnC0g8eHEQDV72kOIhYsB3HRbg7EitF4dF6\n" +
            "TnQQWRZAisgS2tfgDbdDdx64Yz64smDhgDPn67iptOgZEBqRbYKzwW90JPHJjdIA\n" +
            "sTbepn+pXNCr2d4W75P8dmCTGco1/eCzYLfoxtAOzboYuZ0fE8vtNvwQBno1i4DE\n" +
            "IAkCAwEAAaOCAxMwggMPMAkGA1UdEwQCMAAwbAYIKwYBBQUHAQEEYDBeMCgGCCsG\n" +
            "AQUFBzABhhxodHRwOi8vb2NzcC5jZmNhLmNvbS5jbi9vY3NwMDIGCCsGAQUFBzAC\n" +
            "hiZodHRwOi8vZ3RjLmNmY2EuY29tLmNuL292b2NhL292b2NhLmNlcjAcBgNVHREE\n" +
            "FTATghFyZXYuY2VibmV0LmNvbS5jbjAOBgNVHQ8BAf8EBAMCBaAwHQYDVR0OBBYE\n" +
            "FE8V45FeA784niUqCEBOwgBRu8XyMB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEF\n" +
            "BQcDATCCAX8GCisGAQQB1nkCBAIEggFvBIIBawFpAHcA6D7Q2j71BjUy51covIlr\n" +
            "yQPTy9ERa+zraeF3fW0GvW4AAAGEN4sLWAAABAMASDBGAiEA1bPCfW8uAbrgMcxy\n" +
            "KwITanGvXPAFW8KwxnpuAVnl48ECIQD3D26bq0m5qm3+m2zM3LnALoLNKIingAAj\n" +
            "fABUPGeDJQB1AK33vvp8/xDIi509nB4+GGq0Zyldz7EMJMqFhjTr3IKKAAABhDeL\n" +
            "FY0AAAQDAEYwRAIgWhCHRM47vNI4TlZudyGtzZincKtVRyU2/r/zMw1sb+MCICXP\n" +
            "L2CtnHa1+gEdZtx0s4LCpljj1t3nhBSNnTNdJMU9AHcAb1N2rDHwMRnYmQCkURX/\n" +
            "dxUcEdkCwQApBo2yCJo32RMAAAGEN4sbeQAABAMASDBGAiEAg7C+rp820TNLCwB6\n" +
            "Yb4fZ/GvS+VJj2EqwmlgbWHsHSACIQD2hHUCEUjTvWl8b1z65kIJiM74BvHzt8/m\n" +
            "JUVX3Y60pTAfBgNVHSMEGDAWgBRms+/7VJWH6aylllau5n3tOtBD0TBGBgNVHSAE\n" +
            "PzA9MDsGBmeBDAECAjAxMC8GCCsGAQUFBwIBFiNodHRwOi8vd3d3LmNmY2EuY29t\n" +
            "LmNuL3VzL3VzLTEyLmh0bTA8BgNVHR8ENTAzMDGgL6AthitodHRwOi8vY3JsLmNm\n" +
            "Y2EuY29tLmNuL09WT0NBL1JTQS9jcmwxODIuY3JsMA0GCSqGSIb3DQEBCwUAA4IB\n" +
            "AQDEnpbli8qZS4qkEtVFM3KxajtajzaZ56qj0ElsXQ3zXikIKYHQ707w1gVrlctp\n" +
            "sKLmLFJCrXTzpE7fzWl9GyVmDM0ZdKO1UIJItcaSdZ17tVNEUI5y0Z48HIpv16eP\n" +
            "ZfYU3Q6Djy/91TfwGv9o19lIB3IV269v7iPYrndSA8844MTS0/y2+evDFlOWqVkM\n" +
            "Drncaf6jhbcNByjJh0JYRUIHDFUA9n/DVCnQCkymDMe+SK6+8K0N12oX86wUoTeb\n" +
            "Qu+FU4bvQacqvi6FN23xkiH353nYLWZZzYhF7IQjTgowVSmXdO3hQcnJ4jntp9hO\n" +
            "GGw2oV8dpSxdOALE1ci3el5C\n" +
            "-----END CERTIFICATE-----";

    public static void main(String[] args) throws Exception {
        ValidatePathWithParams pathValidator = new ValidatePathWithParams(null);

        if (args.length >= 1 && "CRL".equalsIgnoreCase(args[0])) {
            pathValidator.enableCRLCheck();
        } else {
            // OCSP check by default
            pathValidator.enableOCSPCheck();
        }

        // Validate valid
        pathValidator.validate(new String[]{VALID, CFCA_EV_OCA},
                ValidatePathWithParams.Status.GOOD, null, System.out);

        // Validate expired
        pathValidator.validate(new String[]{EXPIRED, CFCA_EV_OCA},
                ValidatePathWithParams.Status.EXPIRED, null, System.out);

        // Validate revoked
        pathValidator.validate(new String[]{REVOKED, CFCA_OV_OCA},
                ValidatePathWithParams.Status.REVOKED,
                "Thu Nov 03 11:41:08 CST 2022", System.out);
    }
}
