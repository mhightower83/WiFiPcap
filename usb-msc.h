#ifndef USB_MSC_H
#define USB_MSC_H

esp_err_t sd_init(void);
void sd_end(void);
bool setupMsc(void);

#endif
