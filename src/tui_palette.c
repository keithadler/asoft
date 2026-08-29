/* The palette both backends load.
 *
 * Sixteen colours is what VGA text mode has, so the design is built to that
 * number rather than to what a terminal could manage; the DOS build then gets
 * the same picture rather than a washed-out approximation of it.
 */
#include "tui.h"

const unsigned char tui_palette[16][3] = {
    {  22,  22,  30 },   /* bg      */
    {  31,  35,  53 },   /* panel   */
    {  86,  95, 137 },   /* dim     */
    { 169, 177, 214 },   /* text    */
    { 192, 202, 245 },   /* bright  */
    { 125, 207, 255 },   /* cyan    */
    { 158, 206, 106 },   /* green   */
    { 224, 175, 104 },   /* yellow  */
    { 187, 154, 247 },   /* purple  */
    { 247, 118, 142 },   /* pink    */
    {  40,  52,  87 },   /* sel     */
    { 255, 255, 255 },   /* white   */
    { 115, 218, 202 },   /* teal    */
    { 219,  75,  75 },   /* red     */
    { 255, 158,  100 },  /* orange  */
    {   0,   0,   0 }    /* black   */
};
