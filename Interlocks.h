/*
  Copyright (C) 2023 - M Hightower

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/
#ifndef INTERLOCKS_H
#define INTERLOCKS_H

// Referenced:
// https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/ip/tensilica-ip/isa-summary.pdf#page=117
// https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/ip/tensilica-ip/isa-summary.pdf#page=120
// less so with esp-idf/components/xtensa/include/xtensa/core-macros.h - which looks broken in a few places.

#if XCHAL_HAVE_RELEASE_SYNC && XCHAL_HAVE_S32C1I && XCHAL_HW_MIN_VERSION_MAJOR >= 2200

////////////////////////////////////////////////////////////////////////////////
//
static inline bool interlocked_compare_exchange(volatile void* *addr, void* const testval, void* const setval) {
    void* setval_oldval = setval;
    __asm__ __volatile__ (
        "    wsr.scompare1 %[testval] \n"
        "    s32c1i %[setval_oldval], %[addr], 0 \n"
        : [setval_oldval]"+&a"(setval_oldval)
        : [testval]"a"(testval), [addr]"a"(addr)
        : "memory");
    return setval_oldval == testval;
}

static inline bool interlocked_compare_exchange(volatile uint32_t *addr, uint32_t const testval, uint32_t const setval) {
    return interlocked_compare_exchange((volatile void* *)addr, (void*)testval, (void*)setval);
}


////////////////////////////////////////////////////////////////////////////////
//
static inline void* interlocked_read(volatile void* *addr) {
    void* val;
    __asm__ __volatile__ (
        "    l32ai   %[val],  %[addr], 0 \n"
        : [val]"=a"(val)
        : [addr]"a"(addr)
        : "memory");
        return val;
}

static inline uint32_t interlocked_read(volatile uint32_t *addr) {
    return (uint32_t)interlocked_read((volatile void**)addr);
}


static inline void * interlocked_exchange(volatile void **addr, void* const newval) {
    void* oldval;
    void* newold;
    __asm__ __volatile__ (
        "1: \n"
        "    mov    %[newold],  %[newval] \n"
        "    l32ai  %[oldval],  %[addr], 0 \n"
        "    wsr.scompare1      %[oldval] \n"
        "    s32c1i %[newold],  %[addr], 0 \n"
        "    bne    %[newold],  %[oldval], 1b \n"
        : [oldval]"=&a"(oldval), [newold]"=&a"(newold)
        : [newval]"a"(newval), [addr]"a"(addr)
        : "memory");
        return oldval;
}

// Untested - WIP - don't need this yet
// static inline void * interlocked_add(volatile void **addr, volatile void **valaddr) {
//     void* oldval;
//     void* newval;
//     __asm__ __volatile__ (
//         "1: \n"
//         "    l32ai  %[newval],   %[valaddr], 0 \n"
//         "    l32ai  %[oldval],  %[addr], 0 \n"
//         "    add    %[newval],  %[oldval] \n"
//         "    wsr.scompare1      %[oldval] \n"
//         "    s32c1i %[newval],  %[addr], 0 \n"
//         "    bne    %[newval],  %[oldval], 1b \n"
//         : [oldval]"=&a"(oldval), [newval]"=&a"(newval)
//         : [addr]"a"(addr), [valaddr]"a"(valaddr)
//         : "memory");
//         return newval;
// }

// Status - not heavly test, but seems to work
static inline void * interlocked_add(volatile void **addr, void* const val) {
    void* oldval;
    void* newval;
    __asm__ __volatile__ (
        "1: \n"
        "    l32ai  %[oldval],  %[addr], 0 \n"
        "    add    %[newval],  %[oldval],  %[val] \n"
        "    wsr.scompare1      %[oldval] \n"
        "    s32c1i %[newval],  %[addr], 0 \n"
        "    bne    %[newval],  %[oldval], 1b \n"
        : [oldval]"=&a"(oldval), [newval]"=&a"(newval)
        : [addr]"a"(addr), [val]"a"(val)
        : "memory");
        return newval;
}
static inline uint32_t interlocked_add(volatile uint32_t *addr, const uint32_t val) {
  return (uint32_t)interlocked_add((volatile void **)addr, (void*)val);
}

static inline void* interlocked_write(volatile void* *addr, void *val) {
    // void* val;
    __asm__ __volatile__ (
        "    s32ri   %[val],  %[addr], 0 \n"
        : [val]"+a"(val)
        : [addr]"a"(addr)
        : "memory");
        return val;
}
static inline uint32_t interlocked_write(volatile uint32_t *addr, uint32_t val) {
    return (uint32_t)interlocked_write((volatile void* *)addr, (void *)val);
}
#endif


#endif
