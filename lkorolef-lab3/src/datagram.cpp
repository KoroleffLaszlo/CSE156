std::vector<uint8_t> Dgram::encode_dgram(uint16_t sequence_num) {
    uint16_t seq_network_order = htons(sequence_num);
    return {
        static_cast<uint8_t>(seq_network_order >> 8),
        static_cast<uint8_t>(seq_network_order & 0xFF)
    };
}