#pragma once
/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include <iostream>
#include <thread>
#include <utility>
#include "test_base.h"
#include "bq_common/encryption/rsa.h"
#include "bq_common/encryption/aes.h"

namespace bq {
    namespace test {

        class test_encryption : public test_base {

        private:
            void test_rsa(test_result& result, int32_t key_bits, int32_t test_count)
            {
                bq::hash_map<int32_t, bq::tuple<bq::string, bq::string>> prepared_keys;
                prepared_keys.add(1024, bq::make_tuple<bq::string, bq::string>(R"(-----BEGIN RSA PRIVATE KEY-----
MIICXQIBAAKBgQC/KY6/zAKjjd+6G256EHBGqyCnfCBGFtAbZX2qb8IuVmnUhoVa
3HfwqQz5IWzm5TKdojUD5L9SYy116HQFERLppq+AY16EeYv0pR61TnyzTgFRRXuu
qmItIwECq2WucZ8Ys3AEQsfDY6Z31OINLx0ftCh7QKZHkPzvJy/y9pgwvQIDAQAB
AoGBAJD0E0GOkiWxLAf0SxaWJnz/wHgf4F0laVKM5/h4XDdE4WT9SFu4t94uYh77
YJfJDlOHr0mviAASImO5C1jYNIzWi+ThvW+8IEgpE8t/+tE8x0BGvwBauQfk397k
vwf/9Yi666tJYT4h7Ny19oDiaU8R5XeV8LmoCdq/5xhHDT7BAkEA36e9YJsZsF6L
xlGUQVQR90sI5bovtu4b7tjrewJCTB29fR7fduk0fayUeiZ2gDyxdCYM7hR/TuAT
XO51nFUjcQJBANrO2g4UUqyeKEpL2JPCNWCUZCaJnNyQSeaaZuDcb5PQrKNtXgiY
xj09w9/+bIjbsau6aAp1Tqj9OtsOL4ohpA0CQBQt1TdXJx0zmpbdG2w9gpV2Tqoz
f0SY/SoypiRmEsc9U7BrTawz5EmGfar7t2tgjn41RKtibA4Yx4Z1+WSOhfECQQCe
RwlHK/5N7a45aDoHUHHqJg14Lw1wI1PB4yjOOcbghw+KvH9L+q9T94zsSA/cxAb2
sUW3Yvn+lgdwFAfhhMo1AkBSVop+u94LplzwfOH5U2q7dWnhZvSY1bMd+d/ZFPar
eSFkEwI+QE7YZLQUCjw/GRTS6LczBS+EOgpfomXN272r
-----END RSA PRIVATE KEY-----
)", R"(ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAAAgQC/KY6/zAKjjd+6G256EHBGqyCnfCBGFtAbZX2qb8IuVmnUhoVa3HfwqQz5IWzm5TKdojUD5L9SYy116HQFERLppq+AY16EeYv0pR61TnyzTgFRRXuuqmItIwECq2WucZ8Ys3AEQsfDY6Z31OINLx0ftCh7QKZHkPzvJy/y9pgwvQ== pippocao@PIPPOCAO-PC6
)"));
                prepared_keys.add(2048, bq::make_tuple<bq::string, bq::string>(R"(-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAsDsX+mR4jZ+E9Y3I7gqAQ20PZ7w59xxMMkQmI4h4f3+h8/gp
jUKnehK3HI46kDvweUdi6QtvwaxEkl2tPLt6HtXvgIRQnHRZkzr85Wpn04zsBRMI
jsbdmgTpBZoXuFE4mysTViuDgzoEqTf+MWaqPjY6cfzUchsI+m9KMVK3TfiyF1OD
YTgc/GqLxfcHW5oLwAFHUU6ErYTtYEtZOcNef177RldpsUbVN4Bra5qZeir1qvq9
6GEgIATuDfuA0O6BrKf5OTZGmV7ppVA+C0Ty07of4hMe1evBTE4egSZJZBOmbQcc
CQ/eDTuGLrdyzzNm1BNmiwn+bp5aHswHcrSqwQIDAQABAoIBAApbrtEczqkkUyBL
sR+splVQN7OVMKMBmsjpkIROJSb5TX+Venu/CzD1oKWhBngrzbqTI5EnBu6PPYiM
0P3c1xSw5l7baBGKFSm+MdcaJfgdNFIoCDy8G5JN75RQtkwzGeyk34IVFKnF4zg+
/kXRDviRH2ZLwgDi7egqWmYDwppmhq5sV+Py0Bt6XgC1taxYs0pGIXqolGCAfPHq
CgzOVkR0MQLFUXsFkC7erhhV7g1ixmBOg5WCZ7pZCsz0dnaH2F6snbWQFzD4YBew
IJyvQDlOOaJF+CsVZVGGuL4VJu83tBTw8Kgjoprg8nVa9+r4C1r5CRn/h3RPV1pK
6lUsoEECgYEA4RbcJSi7j2HflYt/OX3hZORSul6ClOk+Oh2FI7LyTUQToBO8IL56
wiwMdfvq6ekdenxNlqm9FDF452GGUKd62JkwxZFvjFxybz1jx14GoQx0pvPuHVwO
Gj4pnjSDjjBdThT7n2+ocLKCVA3PTOKb4LSBpxfO5uIrTWFE/EJFgvUCgYEAyG6W
USzgPOc0d3xqFpGI01ImdYCdvsfwvTP0d4fasvCyZzqxVSd/COoIfzRnOEBzC9ut
KRqe6drS7Y1nZ0HBW4zjGw2812jQwrWIfUTHbi+VBx6NlTX5RdS4J7j4h1bkpajK
hDybnU26K5hwK+jfaTZnvadK8hOS+PW750ueYR0CgYEArhtlm6SfN/BH5r/pYAob
v1PRHfGe5hpolMFiy9NOEzAWUhsRyO4tvGYgGV8MPXSg5a6iwWh4JdeCos9P+rEh
l8se66NselDD/4Gn3X3AePBOhxll1PXwvqHYxVPvcpu8gHpAi/nte4bIwsWP1PPw
COb25s3Gr9bAEfGsT8ffRsUCgYAbONHXopWuD+TTWaV6/TctJFou2VITaEptGBJa
7aupZAGG/bS6EQwy5L5UsIRYYoB9ms9w5rmwn1TIiM0DfC3Lxl3N/dapFwZLe6ZX
BTFk6Ld/6Qlnu5XogxSj6H5wuz+AtGUVArpL6hOf+is5+33kZ0w9uOv35uquP4nO
xzlyjQKBgQCS5jIIWeN5wODwa+N2pZ6JrnooN/NmF+5ElfoToLh3Yb/yYqYQSM8e
4mDhezXYzHmEuHxQp9URDdcoGcLxcH+eg1MZvuUVt0OdIdb8Ai7zXG1zw7QNWK6j
4lI5KbODGCPuKpSUdRbJLaCvwT0MgO18buV4Z9e+qZKTdwYaZtW6dw==
-----END RSA PRIVATE KEY-----
)", R"(ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwOxf6ZHiNn4T1jcjuCoBDbQ9nvDn3HEwyRCYjiHh/f6Hz+CmNQqd6ErccjjqQO/B5R2LpC2/BrESSXa08u3oe1e+AhFCcdFmTOvzlamfTjOwFEwiOxt2aBOkFmhe4UTibKxNWK4ODOgSpN/4xZqo+Njpx/NRyGwj6b0oxUrdN+LIXU4NhOBz8aovF9wdbmgvAAUdRToSthO1gS1k5w15/XvtGV2mxRtU3gGtrmpl6KvWq+r3oYSAgBO4N+4DQ7oGsp/k5NkaZXumlUD4LRPLTuh/iEx7V68FMTh6BJklkE6ZtBxwJD94NO4Yut3LPM2bUE2aLCf5unloezAdytKrB pippocao@PIPPOCAO-PC6
)"));
                prepared_keys.add(3072, bq::make_tuple<bq::string, bq::string>(R"(-----BEGIN RSA PRIVATE KEY-----
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
-----END RSA PRIVATE KEY-----
)", R"(ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQCogUHcEPMw6nSBE7ByigDoEFTOAAwcfcSjmMshrzmvTooJ7aqI04XY+ROtaXrv9bY+p1frodXOv1MJBixn8omWeqKrolmMwEFEg96tlwKgmKojyiP2XG/Pu3kE4gBaex/UnG2D7QC8synJeSY3QDTq6x8EgZbqkfuHaAiLANWybo3lnBR5mRlhDaR3eBX//6wGQAfFRC26m7gnODQ2g7a/4yOis38OH0UZxA54rJT+vNrKSGdKtjWW5TXGEEMBDOdJlgpI6DydM05G6+t7Vrf3l5IKmPiY63GBHSbY7WhHyyHLhj2HMUe8P2Acr8j2VP5dZDy2ub/gD2omVH6CxTSpgGojcPSCRbq/g2v9GhzmI086BbM2SrX+BOorGK5Y/D32GknGQqkOulkA2tbXcr+QRjKILo2jxHU9jyphJTcJchptVT4lW5ctC1Lx3pXPFY/2YFJl/ZO/987s3DtG8/vfcPDIu9oeb4zw9y/FCjpEN42WpyNAuxFrf5Hw0o/0DM0= pippocao@PIPPOCAO-PC6
)"));

                prepared_keys.add(4096, bq::make_tuple<bq::string, bq::string>(R"(-----BEGIN RSA PRIVATE KEY-----
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
-----END RSA PRIVATE KEY-----
)", R"(ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAACAQCr8m4sHOJ8Pqed2nFhNO/0M/0X6vG4iNZePjdPy+osKUumD/7UfLwKoJT+zBx4W5HYJ5UiJcexcwdlB364l4ZkoOuI6asmRwwHELZ66yLrVYkZXfHZhlFVUSODLzo97DkIcRFG8hujxeUPmdo+/mBrfEfIED0YFWXMvmOP/asNkeLP8fsFje1Q/QXk813eJAT1FoKY/eH6aZ+bSRwCC1KNMBCb0qkfcpgtk+BlHqgo1NCfwGCY12+FOHca+4FlXAASTvmumJ34gXwjtrqmeKlwsQ8f78erVs0dMPJg/eXH4rt5X8NkRdNLKxE7IW42StStVApV4a40Cclh0SygKKAwbdgIScZFI3rRIFbEqOsQrArQt7eNeUpd1Ny5OAO7Vy+xEsr40FYlJh2GjJwX/G+US5se2IS01dvBEP519JSzVm4X0tOpAYiloKqoiVi4hmI2y0HnmkRoYlGvP7AUoY/chDhX9cWsdPTpCY2GUIIX3gYnHpA2N/dEgkMEFd0b1NTihdM3FQ1AB59H/CbszbE9xrcICfZwGDE2Q9zP9gWTOZedFtCwEtqWRZjxOk/muYLH+OWZstssazKgMLeZsunBMBhPl5PdVpovSUdvZCzU132ooOyo3ZVsxeER+mXhcvgOK9lK+c1c2/LyDQdx5XWLK81w6+n3RaL+sdWYWyF5SQ== pippocao@PIPPOCAO-PC6
)"));

                prepared_keys.add(7680, bq::make_tuple<bq::string, bq::string>(R"(-----BEGIN RSA PRIVATE KEY-----
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
-----END RSA PRIVATE KEY-----
)", R"(ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAADwQDn1/ZihFPT0Doh/qvaAwBEbX/I3Iw5jyzeq2IWec0Uhr6v9KZA489UvhpVKRpiGGOsWyVZQUxfx8poW3I3+FfPAe8RHW+PsaSEnlIv+Ip348RnZxmS2g3dgS3J2Fs97U441gWwAXTTIvLKXA1YoqUxjvDdrh7e8843RaYFuSMwfob8gw5iyuia/LKye5xXxEKLRieQfmWoLpXL8j61JSIoJwQLqj9hAAhGgreLq5MO6RtUWYNQ9pW3XxfTWMMBXjKiob/zDcppu9BcxgkDwJI+lSp2HUTTFXZcbSvdF5K5i+CY82BRT7Y6ohykqsJUcZHYUwW1Rme7Mk/Ei5g6iIqqTFud4e9GR5eu10H4y7liUrnKWf2hrJA8KXveQYwudfKqVsAomNhAjKQGi5J92FhRCkCbuZiFX3yuLTc/NrZsJVndLIVQdggclz/Fy78FuXA7b/fmWvGYyU9TfWmJbPGV0FGuo58ANHXrOw8lqNjrckVCpDA94wDRd1Im+Keqj+NJuj59r9njSuRpMLSNDpPcOvNrnfAK8L/6nwEbBdF5kkBmGfXflbKF7ZmG+f895jVosCrtW5//7lEDkfPn+5xW34ZtrwdVbbA/tkA+vwA9LKKwL27CWDyjMV9qXLMUg41cfEsf1yxQzcXvv+IePyTJ8oHLwNjxxpQ0ChgRKbDxz7iKZA86T01D46ki+1lpHwaaJ0SSSek0o8vYWo19Tt+fPubT9hQhA1pPx9mwmSr/6HYjRAJRdfAXcZj/0WA43aC3wb5MbQyp59+esJ7+YrQDq3Lhtzxx5eqBLwe6XSnZDASSVC0yqw3sTfWZPLpPsrn6+RYgpwgKMLNNyH2CGExulaT5Y4dWUfkLcHW864jo3r1nIr4NufUox/lTgByTHtI9+1M0Am6SlULf9peAk+F9xUUs2n5AgUTjLomyeY826/CRg6JD5KL4ljjkZ4fL66Fi6ofPJrbIbSvK0V4ec29yu0cImOO/66iCIfv/52smWykqmNVCtuiPB5fvBxCwnEdqAjmYlU60fBXKxZuTFDx2/cd+Szx4vjYH8v2z1E8/N+kWK58J9S/FNq9wrM/ls5GIB3YPcIJ20s/l62rpMSrkK1B/e4+yf4v6e5Ho8lyS3k9UfJiuWhVcX7Boo0W3/lnyhhOnLAR+G5XBQAUK+3o/Z9ZO+IoUxKmAyMpbQbxyrcUFFAhIeiHLakW74YN6M6X69e46WUQlmm2LSZUqSMMxkUjPMOG70gUmGXXYUGb8iPcV2XPEIPeQPsGbKMPkzr8= pippocao@PIPPOCAO-PC6
)"));

                bq::string base_dir = bq::file_manager::get_base_dir(1);
                char key_bits_str[32];
                snprintf(key_bits_str, sizeof(key_bits_str), "%" PRId32 "", key_bits);
                char thread_id_str[64];
                snprintf(thread_id_str, sizeof(thread_id_str), "%" PRIu64 "", bq::platform::thread::get_current_thread_id());
                bq::string parent_dir = bq::file_manager::combine_path(base_dir, bq::string("enc_output"));
                bq::string output_dir = bq::file_manager::combine_path(parent_dir, bq::string("rsa_") + key_bits_str + "_tid_" + thread_id_str);
                bq::file_manager::create_directory(output_dir);
                bq::string std_out_file = bq::file_manager::combine_path(output_dir, "stdout.txt");
                bq::string std_err_file = bq::file_manager::combine_path(output_dir, "stderr.txt");
                for (int32_t i = 0; i < test_count; ++i) {
                    bq::file_manager::remove_file_or_dir(output_dir);
                    bq::file_manager::create_directory(output_dir);
                    bq::string cmd = "ssh-keygen -t rsa -b " + bq::string(key_bits_str) + " -m PEM -N \"\" -f \"" + bq::file_manager::combine_path(output_dir, "id_rsa") + "\" >\"" + std_out_file + "\" 2>\"" + std_err_file + "\"";
                    int32_t ret = system(cmd.c_str());
                    bq::string pub_key_text = bq::file_manager::read_all_text(bq::file_manager::combine_path(output_dir, "id_rsa.pub"));
                    bq::string pri_key_text = bq::file_manager::read_all_text(bq::file_manager::combine_path(output_dir, "id_rsa"));
                    if (ret != 0) {
                        bq::string err_str = bq::file_manager::read_all_text(std_err_file);
                        bq::string out_str = bq::file_manager::read_all_text(std_out_file);
                        bq::util::log_device_console(bq::log_level::warning, "ssh-key-gen failed out:%s, use pre generated key instead", out_str.c_str());
                        pub_key_text = bq::get<1>(prepared_keys[key_bits]);
                        pri_key_text = bq::get<0>(prepared_keys[key_bits]);
                    }
                    else {
                        pub_key_text = bq::file_manager::read_all_text(bq::file_manager::combine_path(output_dir, "id_rsa.pub"));
                        pri_key_text = bq::file_manager::read_all_text(bq::file_manager::combine_path(output_dir, "id_rsa"));
                    }
                    bq::rsa::public_key pub;
                    bq::rsa::private_key pri;
                    bool parse_pub_result = bq::rsa::parse_public_key_ssh(pub_key_text, pub);
                    result.add_result(parse_pub_result, "RSA_%" PRId32 " parse public key failed at iteration %" PRId32 "", key_bits, i);
                    bool parse_pri_result = bq::rsa::parse_private_key_pem(pri_key_text, pri);
                    result.add_result(parse_pri_result, "RSA_%" PRId32 " parse private key failed at iteration %" PRId32 "", key_bits, i);
                    if (!parse_pub_result || !parse_pri_result) {
                        continue;
                    }
                    bq::util::srand(static_cast<uint32_t>(bq::platform::high_performance_epoch_ms()));
                    bq::array<uint8_t> plaintext;
                    uint32_t size = bq::max_value(static_cast<uint32_t>(sizeof(uint32_t)), bq::util::rand() % bq::min_value(static_cast<uint32_t>(pub.n_.size()), 1024U));
                    size = static_cast<uint32_t>((size / sizeof(uint32_t)) * sizeof(uint32_t));
                    plaintext.clear();
                    plaintext.fill_uninitialized(size);
                    for (uint32_t j = 0; j < size / sizeof(uint32_t); ++j) {
                        *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(plaintext.begin()) + j * sizeof(uint32_t)) = bq::util::rand();
                    }
                    // Raw RSA (no padding) returns the minimal big-endian form on decrypt; leading 0x00 bytes are not preserved.
                    // To avoid false negatives in tests, we force the first byte to be non-zero here.
                    if (plaintext[0] == 0) {
                        plaintext[0] = bq::max_value(static_cast<uint8_t>(1), static_cast<uint8_t>(bq::util::rand() & static_cast<uint32_t>(0xff)));
                    }
                    bq::array<uint8_t> ciphertext;
                    bool enc_result = bq::rsa::encrypt(pub, plaintext, ciphertext);
                    result.add_result(enc_result, "RSA_%" PRId32 " encryption test failed at iteration %" PRId32 "", key_bits, i);
                    if (enc_result) {
                        bq::array<uint8_t> decrypted_text;
                        bool dec_result = bq::rsa::decrypt(pri, ciphertext, decrypted_text);
                        result.add_result(dec_result, "RSA_%" PRId32 " decryption test failed at iteration %" PRId32 "", key_bits, i);
                        if (dec_result) {
                            if (decrypted_text.size() != plaintext.size()) {
                                result.add_result(false, "RSA_%" PRId32 " decryption size mismatch at iteration %" PRId32 "", key_bits, i);
                            } else {
                                bool content_match = (memcmp((const uint8_t*)decrypted_text.begin(), (const uint8_t*)plaintext.begin(), plaintext.size()) == 0);
                                result.add_result(content_match, "RSA_%" PRId32 " decryption content mismatch at iteration %" PRId32 "", key_bits, i);
                            }
                        }
                    }
                }
            }

            void test_aes(test_result& result, aes::enum_key_bits key_bits)
            {
                bq::util::srand(static_cast<uint32_t>(bq::platform::high_performance_epoch_ms()));

                bq::array<uint8_t> plaintext;
                bq::array<uint8_t> ciphertext;
                bq::array<uint8_t> decrypted_text;
                for (int32_t i = 0; i < 8; ++i) {
                    uint32_t size = bq::max_value(1u, bq::util::rand() % (64 * 1024)) * 16;
                    plaintext.clear();
                    plaintext.fill_uninitialized(size);
                    for (uint32_t j = 0; j < size / sizeof(uint32_t); ++j) {
                        *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(plaintext.begin())) = bq::util::rand();
                    }
                    bq::aes aes(bq::aes::enum_cipher_mode::AES_CBC, key_bits);
                    auto key = aes.generate_key();
                    auto iv = aes.generate_iv();
                    bool enc_result = aes.encrypt(key, iv, plaintext, ciphertext);
                    result.add_result(enc_result
                            && (memcmp((const uint8_t*)ciphertext.begin(), (const uint8_t*)plaintext.begin(), bq::min_value(plaintext.size(), ciphertext.size())) != 0),
                        "AES_%" PRId32 " encryption test : %" PRId32 "", static_cast<int32_t>(key_bits), i);

                    if (enc_result) {
                        bool dec_result = aes.decrypt(key, iv, ciphertext, decrypted_text);
                        result.add_result(dec_result, "AES_%" PRId32 " decryption test : %" PRId32 "", static_cast<int32_t>(key_bits), i);
                        if (dec_result) {
                            if (decrypted_text.size() != plaintext.size()) {
                                result.add_result(false, "AES_%" PRId32 " decryption size mismatch at iteration %" PRId32 "", static_cast<int32_t>(key_bits), i);
                            } else {
                                bool content_match = (memcmp((const uint8_t*)decrypted_text.begin(), (const uint8_t*)plaintext.begin(), plaintext.size()) == 0);
                                result.add_result(content_match, "AES_%" PRId32 " decryption content mismatch at iteration %" PRId32 "", static_cast<int32_t>(key_bits), i);
                            }
                        }
                    }
                }
            }

        public:
            virtual test_result test() override
            {
                test_result result;
                constexpr uint32_t thread_count = 2;
                test_output(bq::log_level::info, "RSA test begin...");
                bq::array<std::thread*> rsa_threads;
                for (uint32_t i = 0; i < thread_count; ++i) {
                    rsa_threads.push_back(new std::thread([&result, this]() {
                        test_rsa(result, 1024, 2);
                        test_rsa(result, 2048, 2);
                        test_rsa(result, 3072, 1);
                        // test_rsa(result, 7680, 1);
                    }));
                }
                for (uint32_t i = 0; i < thread_count; ++i) {
                    rsa_threads[i]->join();
                    delete rsa_threads[i];
                }

                test_output(bq::log_level::info, "AES test begin...");
                bq::array<std::thread*> aes_threads;
                for (uint32_t i = 0; i < thread_count; ++i) {
                    aes_threads.push_back(new std::thread([&result, this]() {
                        test_aes(result, bq::aes::enum_key_bits::AES_128);
                        test_aes(result, bq::aes::enum_key_bits::AES_192);
                        test_aes(result, bq::aes::enum_key_bits::AES_256);
                    }));
                }
                for (uint32_t i = 0; i < thread_count; ++i) {
                    aes_threads[i]->join();
                    delete aes_threads[i];
                }
                return result;
            }
        };
    }
}
