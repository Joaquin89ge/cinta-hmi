#pragma once

// Tres FIFO, uno por sentido: navegación y estado entran, las órdenes salen.
void openFifos();
void pollFifos();
void writeOrder(const char* line);
