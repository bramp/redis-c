#ifndef LIBREDIS_PRIVATE_H_
#define LIBREDIS_PRIVATE_H_

#define UNKNOWN_READ_LENGTH 128 /** How much should we read when we don't know the length of the data */

#define STATE_WAITING         0 /** We are waiting for the first line of the reply */
#define STATE_READ_BULK       1 /** We reading a bulk reply                        */
#define STATE_READ_MULTI_BULK 2 /** We reading a multi-mulk reply                  */

#endif /* LIBREDIS_PRIVATE_H_ */
