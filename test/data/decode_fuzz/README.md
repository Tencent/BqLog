# Decoder regression cases

Files here are inputs a fuzzer produced, kept as they were found: they are not valid logs and must
not be turned into valid ones, because the point of each is the exact byte pattern that used to make
the decoder do something it should not.

## `v11_enc_resync.logcompr`

Written by `tools/decoder_fuzz` (runtime mode) by mutating a real v11 encrypted file, sha256
`bc43ce1bb9827e841eb79623308ae759e7c8dccd2046f73e120941b38f045c77`, 3633 bytes. It decodes with
`test/data/legacy/legacy_key.pem`.

Segment 2 of 3 carries an item whose declared size does not fit, so the record layer fails, and the
decoder's `decode()` resynchronises to the next segment. That path used to leave the previous
segment's read cache in place; the following fill rewound the file cursor into the new segment's
header, which sits before its payload, and the ChaCha keystream offset was computed from a position
the segment does not have. In a debug build that was an assertion
(`decoder chunk starts before the segment payload`); with assertions compiled out it decrypted the
header bytes and fed them to the record layer as if they were log data.

The test asserts that decoding this file fails cleanly.
