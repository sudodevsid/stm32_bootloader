function hexToBin(hexBuffer, fillByte = 0xff) {
  const lines = hexBuffer.toString('ascii').split(/\r?\n/);
  let extendedAddr = 0;
  const chunks = [];

  for (const line of lines) {
    if (!line.startsWith(':')) continue;

    const byteCount = parseInt(line.substr(1, 2), 16);
    const address = parseInt(line.substr(3, 4), 16);
    const recordType = parseInt(line.substr(7, 2), 16);
    const dataHex = line.substr(9, byteCount * 2);

    if (recordType === 0x00) {
      chunks.push({ address: extendedAddr + address, data: Buffer.from(dataHex, 'hex') });
    } else if (recordType === 0x01) {
      break;
    } else if (recordType === 0x02) {
      extendedAddr = parseInt(dataHex, 16) * 16;
    } else if (recordType === 0x04) {
      extendedAddr = parseInt(dataHex, 16) << 16;
    }
  }

  if (chunks.length === 0) return Buffer.alloc(0);

  const minAddr = Math.min(...chunks.map((c) => c.address));
  const maxAddr = Math.max(...chunks.map((c) => c.address + c.data.length));

  const bin = Buffer.alloc(maxAddr - minAddr, fillByte);
  for (const { address, data } of chunks) {
    data.copy(bin, address - minAddr);
  }

  return bin;
}

module.exports = { hexToBin };
