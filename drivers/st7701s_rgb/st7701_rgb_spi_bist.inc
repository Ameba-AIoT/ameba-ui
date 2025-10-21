    spi_write_command(0xFF);
    spi_write_data(0x77);
    spi_write_data(0x01);
    spi_write_data(0x00);
    spi_write_data(0x00);
    spi_write_data(0x12);

    spi_write_command(0xD1);
    spi_write_data(0x81);
    spi_write_data(0x15);
    spi_write_data(0x01);
    spi_write_data(0xE0);
    spi_write_data(0x0C);
    spi_write_data(0x01);
    spi_write_data(0xF0);
    spi_write_data(0x01);
    spi_write_data(0xE0);
    spi_write_data(0xF0);
    spi_write_data(0x01);
    spi_write_data(0xE0);
    spi_write_data(0x01);
    spi_write_data(0xE0);

    spi_write_command(0xD2);
    spi_write_data(0x06);       // chess pattern
    spi_write_data(0x00);