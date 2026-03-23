spi_write_command(spi, 0xFF);
spi_write_data(spi, 0x77);
spi_write_data(spi, 0x01);
spi_write_data(spi, 0x00);
spi_write_data(spi, 0x00);
spi_write_data(spi, 0x12);

spi_write_command(spi, 0xD1);
spi_write_data(spi, 0x81);

spi_write_command(spi, 0xD2);
spi_write_data(spi, 0x06);
