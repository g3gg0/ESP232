


void led_setup()
{
}


void led_set(uint8_t n, uint8_t r, uint8_t g, uint8_t b)
{
    digitalWrite(LED_PIN, ((r>0) || (g>0) || (b>0)) ? HIGH : LOW );
}

bool led_loop()
{
    return false;
}
