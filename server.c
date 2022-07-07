#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "Train.c"
#include "Account.c"
#include "Booking.c"
#include "initial_setup.c"

#define PORT 43456

void client_workplace(int desc, int try_count, struct Account user);
void admin_workplace(int desc, int try_count);



void send_message(int desc, char *msg, char *input){
    for (size_t i = 0; i < 10000; i++);    
    write(desc, msg, 1024);
    read(desc, input, 1024);
}

struct flock file_read_lock(struct flock lock, int fd){
    lock.l_type = F_RDLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_pid = getpid();
	printf("Before Entering the RL critical section\n");
	fcntl(fd, F_SETLKW, &lock);
    printf("Inside Critical Section\n");
    return lock;
}

struct flock file_read_unlock(struct flock lock, int fd){
    lock.l_type = F_UNLCK;
	printf("Unlocked\n");
	fcntl(fd, F_SETLK, &lock);
    return lock;
}

struct flock file_write_lock(struct flock lock, int fd){
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_pid = getpid();
	printf("Before Entering the WL critical section\n");
	fcntl(fd, F_SETLKW, &lock);
    printf("Inside Critical Section\n");
    return lock;
}

struct flock file_write_unlock(struct flock lock, int fd){
    lock.l_type = F_UNLCK;
	printf("Unlocked\n");
	fcntl(fd, F_SETLK, &lock);
    return lock;
}

void session_logout(int desc, struct Account user){
    struct Account temp;
    int index = 0;
    char input[1024];
    struct flock lock;
    int fd = open("login.dat", O_RDWR);
    lock = file_write_lock(lock, fd);
    while(read(fd, &temp, sizeof(temp)) > 0){
        if(strcmp(temp.username, user.username)==0){
            user.session = 0;
            lseek(fd, index*sizeof(user), SEEK_SET);
            write(fd, &user, sizeof(user));
            break;            
        }
        ++index;
    }
    lock = file_write_unlock(lock, fd);
    close(fd);
    send_message(desc, "CLOSE", input);
}

int operation_bid(){
    int bid, temp;
    struct flock lock;
    int fd = open("bid.dat", O_RDWR);
    lock = file_write_lock(lock, fd);
	read(fd, &bid, sizeof(bid));
    temp = bid;
	++bid;
	lseek(fd, 0L, SEEK_SET);
	write(fd, &bid, sizeof(bid));
    lock = file_write_unlock(lock, fd);
    close(fd);
    return temp;
}

int print_trains(int desc){
    struct Train train;
    struct flock lock;
    char buff[1024];
    int fd = open("train.dat", O_RDONLY);
    lock = file_read_lock(lock, fd);
    int flag = 0;
    char message[1024] = "\nTrain Details are:";
    send_message(desc, "R", buff);
    send_message(desc, message, buff);
    while(read(fd, &train, sizeof(train)) > 0){
        if(strcmp(train.status, "ACTIVE")==0){
            strcpy(message, train.number);
            strcat(message, "(number)\t");
            strcat(message, train.name);
            strcat(message, "(name)\t");
            snprintf(buff, sizeof(buff), "%d", train.total_seats);
            strcat(message, buff);
            strcat(message, "(total seats)\t");
            snprintf(buff, sizeof(buff), "%d", train.booked_seats);
            strcat(message, buff);
            strcat(message, "(booked seats)\t");
            send_message(desc, "R", buff);
            send_message(desc, message, buff);
            flag = 1;
        }
    }
    lock = file_read_unlock(lock, fd);
    close(fd);
    if(!flag){
        strcpy(message, "**No Train found. Please check with admin.\n");
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        return 0;
    }
    return 1;
}

int check_seats_train(struct Booking book){
    struct Train train;
    struct flock lock;
    int index = 0, seats_rem;
    int fd = open("train.dat", O_RDWR);
    lock = file_write_lock(lock, fd);
    while(read(fd, &train, sizeof(train))>0){
        if(strcmp(train.status, "ACTIVE")==0 && strcmp(train.number, book.Tnumber)==0){
            seats_rem = train.total_seats - train.booked_seats;
            if(book.seats<=seats_rem){
                train.booked_seats += book.seats;
                lseek(fd, index*sizeof(train), SEEK_SET);
                write(fd, &train, sizeof(train));
                lock = file_write_unlock(lock, fd);
                close(fd);
                return 1;
            }
            else{
                lock = file_write_unlock(lock, fd);
                close(fd);
                return 0;
            }
        }
        ++index;
    }
    lock = file_write_unlock(lock, fd);
    close(fd);
    return 2;
}

int update_seats_train(struct Booking book){
    struct Train train;
    struct flock lock;
    int index = 0;
    int fd = open("train.dat", O_RDWR);
    lock = file_write_lock(lock, fd);
    while(read(fd, &train, sizeof(train))>0){
        if(strcmp(train.status, "ACTIVE")==0 && strcmp(train.number, book.Tnumber)==0){
            train.booked_seats = train.booked_seats + book.seats;
            lseek(fd, index*sizeof(train), SEEK_SET);
            write(fd, &train, sizeof(train));
            lock = file_write_unlock(lock, fd);
            close(fd);
            return 1;
        }
        ++index;
    }
    lock = file_write_unlock(lock, fd);
    close(fd);
    printf("Train Not found in update seats train\n");
    return 0;
}

void ticket_booking(int desc, int try_count, struct Account user){
    char buff[1024];
    struct flock lock;
    struct Booking book, temp;
    int fd;
    if(try_count<=0){
        char message[1024] = "\n**Maximum invalid input limit reached. Shutting down.";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        session_logout(desc, user);
        return;
    }
    if(print_trains(desc)){
        char message[1024] = "\nBook A Ticket- Add Booking Details";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter Train Number: ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(book.Tnumber, buff);
        strcpy(message, "Enter to be booked number of seats in the train(>0)(subject to availability): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        sscanf(buff, "%d", &book.seats);
        book.Bnumber = operation_bid();
        strcpy(book.Anumber, user.username);
        strcpy(book.status, "CONFIRMED");
        if(book.seats<=0){
            strcpy(message, "\n**Invalid Number of seats entered, Please try again.");
            send_message(desc, "R", buff);
            send_message(desc, message, buff);
            client_workplace(desc, --try_count, user);
        }
        else{
            switch (check_seats_train(book)){
            case 0:
                strcpy(message, "\n**");
                snprintf(buff, sizeof(buff), "%d", book.seats);
                strcat(message, buff);
                strcat(message, " Seats not available on Train Number ");
                strcat(message, book.Tnumber);
                strcat(message, ", Please try again.");
                send_message(desc, "R", buff);
                send_message(desc, message, buff);
                client_workplace(desc, --try_count, user);
                return;
            case 1:
                fd = open("booking.dat", O_RDWR);
                lock = file_write_lock(lock, fd);
                while(read(fd, &temp, sizeof(temp))>0);
                write(fd, &book, sizeof(book));
                lock = file_write_unlock(lock, fd);
                close(fd);
                strcpy(message, "\n*Booking with Booking ID - ");
                snprintf(buff, sizeof(buff), "%d", book.Bnumber);
                strcat(message, buff);
                strcat(message, ", successfully booked, Number of seats booked are ");
                snprintf(buff, sizeof(buff), "%d", book.seats);
                strcat(message, buff);
                strcat(message, " on train ");
                strcat(message, book.Tnumber);
                strcat(message, "\n");
                send_message(desc, "R", buff);
                send_message(desc, message, buff);
                client_workplace(desc, 2, user);
                return;
            case 2:
                strcpy(message, "\n**Train Number ");
                strcat(message, book.Tnumber);
                strcat(message, " NOT FOUND. Please try again.");
                send_message(desc, "R", buff);
                send_message(desc, message, buff);
                client_workplace(desc, --try_count, user);
                return;
            }
        }
    }else{
        session_logout(desc, user);
        return;
    }
}

int print_bookings(int desc, struct Account user){
    struct Booking book;
    struct flock lock;
    int fd = open("booking.dat", O_RDONLY); 
    lock = file_read_lock(lock, fd);
    int flag = 0;
    char input[1024];
    char msg[1024] = "\nView Previous Bookings - User ";
    strcat(msg, user.username);
    send_message(desc, "R", input);
    send_message(desc, msg, input);
    while(read(fd, &book, sizeof(book)) > 0){
        if(strcmp(book.Anumber, user.username)==0){
            snprintf(input, sizeof(input), "%d", book.Bnumber);
            strcpy(msg, input);
            strcat(msg, "(booking ID)\t");
            strcat(msg, book.Tnumber);
            strcat(msg, "(train)\t");
            strcat(msg, book.Anumber);
            strcat(msg, "(username)\t");
            snprintf(input, sizeof(input), "%d", book.seats);
            strcat(msg, input);
            strcat(msg, "(seats booked)\t");
            strcat(msg, book.status);
            strcat(msg, "(status)\t");
            send_message(desc, "R", input);
            send_message(desc, msg, input);
            flag = 1;
        }
    }
    lock = file_read_unlock(lock, fd);
    close(fd);
    if(!flag){
        strcpy(msg, "**No Booking found. Please book first.\n");
        send_message(desc, "R", input);
        send_message(desc, msg, input);
        return 0;
    }
    strcpy(msg, "\n");
    send_message(desc, "R", input);
    send_message(desc, msg, input);
    return 1;

}

void update_ticket_booking(int desc, int try_count, struct Account user){
    char buff[1024];
    struct flock lock;
    int flag = 0, index = 0, diff = 0;
    struct Booking book, temp;
    if(try_count<=0){
        char message[1024] = "\n**Maximum invalid input limit reached. Shutting down.";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        session_logout(desc, user);
        return;
    }

    print_trains(desc);
    if(print_bookings(desc, user)){
        char message[1024] = "Update A Ticket Booking(Seats)- Add Booking Details";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter Booking Number: ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        sscanf(buff, "%d", &book.Bnumber);
        strcpy(message, "Enter to be booked number of seats in the train(>0)(subject to availability)(new): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        sscanf(buff, "%d", &book.seats);
        if(book.seats<=0){
            strcpy(message, "\n**Invalid Number of seats entered, Please try again.");
            send_message(desc, "R", buff);
            send_message(desc, message, buff);
            update_ticket_booking(desc, --try_count, user);
        }
        else{
            int fd = open("booking.dat", O_RDONLY);
            lock = file_read_lock(lock, fd);
            while(read(fd, &temp, sizeof(temp))>0){
                if(strcmp(temp.status, "CONFIRMED")==0 && temp.Bnumber == book.Bnumber){
                    flag = 1;
                    temp.seats = book.seats - temp.seats;
                    break;
                }
                ++index;
            }
            lock = file_read_unlock(lock, fd);
            close(fd);        
            if(!flag){
                strcpy(message, "\n**Invalid Booking Number entered or Booking is Cancelled, Please Enter valid Booking Number");
                send_message(desc, "R", buff);
                send_message(desc, message, buff);
                client_workplace(desc, --try_count, user);
            }
            else{
                switch (check_seats_train(temp)){
                    case 0:
                        strcpy(message, "\n**");
                        snprintf(buff, sizeof(buff), "%d", temp.seats);
                        strcat(message, buff);
                        strcat(message, " Seats not available on Train Number ");
                        strcat(message, book.Tnumber);
                        strcat(message, ", Please try again.");
                        send_message(desc, "R", buff);
                        send_message(desc, message, buff);
                        client_workplace(desc, --try_count, user);
                        return;
                    case 1:
                        fd = open("booking.dat", O_RDWR);
                        lock = file_write_lock(lock, fd);
                        temp.seats = book.seats;
                        lseek(fd, index*sizeof(temp), SEEK_SET);
                        write(fd, &temp, sizeof(temp));
                        lock = file_write_unlock(lock, fd);
                        close(fd);
                        strcpy(message, "\n*Booking with Booking ID - ");
                        snprintf(buff, sizeof(buff), "%d", temp.Bnumber);
                        strcat(message, buff);
                        strcat(message, ", successfully updated, Now Number of seats booked are ");
                        snprintf(buff, sizeof(buff), "%d", temp.seats);
                        strcat(message, buff);
                        strcat(message, " on train ");
                        strcat(message, temp.Tnumber);
                        strcat(message, "\n");
                        send_message(desc, "R", buff);
                        send_message(desc, message, buff);
                        client_workplace(desc, 2, user);
                        return;
                    case 2:
                        strcpy(message, "\n**Train Number ");
                        strcat(message, temp.Tnumber);
                        strcat(message, " NOT FOUND. Please try again.\n");
                        send_message(desc, "R", buff);
                        send_message(desc, message, buff);
                        return;
                }


            }
        }
    }else{
        session_logout(desc, user);
        return;
    }
}

void delete_ticket_booking(int desc, int try_count, struct Account user){
    char buff[1024];
    struct flock lock;
    int flag = 0, index = 0;
    struct Booking book, temp;
    if(try_count<=0){
        char message[1024] = "\n**Maximum invalid input limit reached. Shutting down.";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        session_logout(desc, user);
        return;
    }
    if(print_bookings(desc, user)){
        char message[1024] = "Delete A Ticket Booking- Add Booking Details";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter Booking Number: ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        sscanf(buff, "%d", &book.Bnumber);
        int fd = open("booking.dat", O_RDONLY);
        lock = file_read_lock(lock, fd);
        while(read(fd, &temp, sizeof(temp))>0){
            if(strcmp(temp.status, "CONFIRMED")==0 && temp.Bnumber == book.Bnumber){
                temp.seats = -temp.seats;
                if(update_seats_train(temp)){
                    strcpy(temp.status, "CANCELLED");
                    temp.seats = 0;
                    flag = 1;
                }
                break;
            }
            ++index;
        }
        lock = file_read_unlock(lock, fd);
        close(fd);
        if(!flag){
            strcpy(message, "\n**Invalid Booking Number entered or Booking is already Cancelled, Please Enter valid Booking Number");
            send_message(desc, "R", buff);
            send_message(desc, message, buff);
            client_workplace(desc, --try_count, user);
        }
        else{
            int fd = open("booking.dat", O_RDWR);
            lock = file_write_lock(lock, fd);
            lseek(fd, index*sizeof(temp), SEEK_SET);
            write(fd, &temp, sizeof(temp));
            lock = file_write_unlock(lock, fd);
            close(fd);
            strcpy(message, "\n*Booking with Booking ID - ");
            snprintf(buff, sizeof(buff), "%d", temp.Bnumber);
            strcat(message, buff);
            strcat(message, ", successfully deleted, On train ");
            strcat(message, temp.Tnumber);
            strcat(message, "\n");
            send_message(desc, "R", buff);
            send_message(desc, message, buff);
            client_workplace(desc, 2, user);
            return;
        }
    }else{
        session_logout(desc, user);
        return;
    }
}

void client_workplace(int desc, int try_count, struct Account user){
    char input[1024];
    int flag = 0;
    if(try_count<=0){
        char message[1024] = "\n**Maximum invalid input limit reached. Shutting down.";
        send_message(desc, "R", input);
        send_message(desc, message, input);
        return;
    }
    char message[1024] = "Press 1 to Book Ticket\nPress 2 to View Previous Bookings\nPress 3 to Update Booking\nPress 4 to Cancel Booking\nPress any other key to Exit\nEnter your choice: ";
    send_message(desc, "RW", input);
    send_message(desc, message, input);
    if(strcmp("1", input)==0){
        ticket_booking(desc, 2, user);
        client_workplace(desc, 2, user);
    }else if(strcmp("2", input) == 0){
        if(!print_bookings(desc, user)){
            client_workplace(desc, --try_count, user);
        }
        client_workplace(desc, 2, user);
    }else if(strcmp("3", input) == 0){
        update_ticket_booking(desc, 2, user);
        client_workplace(desc, 2, user);
    }else if(strcmp("4", input) == 0){
        delete_ticket_booking(desc, 2, user);
        client_workplace(desc, 2, user);        
    }else{
        session_logout(desc, user);
        return;
    }
}

void admin_train_modify(int desc, int try_count, struct Train temp, int index){
    char buff[1024];
    struct flock lock;
    int flag = 0;
    if(try_count<=0){
        char message[1024] = "\n**Maximum invalid input limit reached. Shutting down.";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        return;
    }
    char message[1024]= "\nTrain Attributes Modification - ";
    strcat(message, temp.number);
    strcat(message, "\nPress 1 to Modify Train Name\nPress 2 to Modify Train Total Seats\nPress 3 to Modify Train Booked Seats\nPress any other key to Exit\nEnter your choice: ");
    send_message(desc, "RW", buff);
    send_message(desc, message, buff);
    if(strcmp("1", buff) == 0){
        strcpy(message, "Enter Train Name (new): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(temp.name, buff);
        strcpy(message, "*Train Name details modified successfully\n");
        flag = 1;
    }
    else if(strcmp("2", buff) == 0){
        int total_seats;
        strcpy(message, "Enter total seats in the train(>0) (new): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        sscanf(buff, "%d", &total_seats);
        if(total_seats<=0){
            strcpy(message, "**Invalid Number of seats entered, Please try again.");
            send_message(desc, "R", buff);
            send_message(desc, message, buff);
            admin_train_modify(desc, --try_count, temp, index);
        }
        else{
            temp.total_seats = total_seats;
            strcpy(message, "*Train total seats details modified successfully.\n");
            flag = 1;
        }
        
    }else if(strcmp("3", buff) == 0){
        int booked_seats;
        strcpy(message, "Enter booked seats in the train(>0) (new): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        sscanf(buff, "%d", &booked_seats);
        if(booked_seats<0 && booked_seats>temp.total_seats){
            strcpy(message, "**Invalid Number of seats entered, Please try again.");
            send_message(desc, "R", buff);
            send_message(desc, message, buff);
            admin_train_modify(desc, --try_count, temp, index);
        }
        else{
            temp.booked_seats = booked_seats;
            flag = 1;
            strcpy(message, "*Train booked seats details modified successfully.\n");
        }
    }else{
        return ;
    }
    if(flag){
        char msg[1024] = "\nTrain Number ";
        char value[100];
        strcat(msg, temp.number);
        strcat(msg, " (");
        strcat(msg, temp.name);
        strcat(msg, ") modified details are:");
        send_message(desc, "R", buff);
        send_message(desc, msg, buff);
        strcpy(msg, temp.number);
        strcat(msg, "\t");
        strcat(msg, temp.name);
        strcat(msg, "\t");
        snprintf(value, sizeof(value), "%d", temp.total_seats);
        strcat(msg, value);
        strcat(msg, "(total seats)\t");
        snprintf(value, sizeof(value), "%d", temp.booked_seats);
        strcat(msg, value);
        strcat(msg, "(booked seats)\t");
        strcat(msg, temp.status);
        send_message(desc, "R", buff);
        send_message(desc, msg, buff);
        int fd = open("train.dat", O_RDWR);
        lock = file_write_lock(lock, fd);
        lseek(fd, index*sizeof(temp), SEEK_SET);
        write(fd, &temp, sizeof(temp));
        lock = file_write_unlock(lock, fd);
        close(fd);
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        admin_workplace(desc, 2);
        return;
    }
}

void admin_train_op(int desc, int try_count){
    char buff[1024];
    int index = 0;
    struct flock lock;
    if(try_count<=0){
        char message[1024] = "\n**Maximum invalid input limit reached. Shutting down.";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        return;
    }
    char message[1024]= "\nTrain Operations\nPress 1 to Add a Train\nPress 2 to Delete a Train\nPress 3 to Update a Train\nPress 4 to Retrieve Train Details\nPress any other key to Exit\nEnter your choice: ";
    struct Train train, temp;
    int flag = 0;
    send_message(desc, "RW", buff);
    send_message(desc, message, buff);
    if(strcmp("1", buff) == 0){
        char message[1024] = "\nOperation Add a Train- Add Train Details";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter Train Number: ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(train.number, buff);
        strcpy(message, "Enter Train Name: ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(train.name, buff);
        strcpy(message, "Enter total seats in the train(>0): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        sscanf(buff, "%d", &train.total_seats);
        if(train.total_seats<=0){
            strcpy(message, "**Invalid Number of seats entered, Please try again.");
            send_message(desc, "R", buff);
            send_message(desc, message, buff);
            admin_train_op(desc, --try_count);
        }
        train.booked_seats = 0;
        strcpy(train.status, "ACTIVE");
        int fd = open("train.dat", O_RDWR);
        lock = file_write_lock(lock, fd);
        while(read(fd, &temp, sizeof(temp)) > 0){
            if(strcmp(train.number, temp.number)==0 && strcmp(temp.status, "ACTIVE")==0){
                char msg[1024] = "**Train Number ";
                strcat(msg, train.number);
                strcat(msg, " is already present. Cannot add Train with same train number, Kindly try again.");
                send_message(desc, "R", buff);
                send_message(desc, msg, buff);
                flag = 1;
                break;
            }
            else if(strcmp(train.number, temp.number)==0 && strcmp(temp.status, "INACTIVE")==0){                        
                lseek(fd, index*sizeof(temp), SEEK_SET);
                write(fd, &train, sizeof(train));
                lock = file_write_unlock(lock, fd);
                close(fd);
                char msg[1024] = "*Train Number ";
                strcat(msg, train.number);
                strcat(msg, " (");
                strcat(msg, train.name);
                strcat(msg, ") is successfully added in the train list.\n");
                send_message(desc, "R", buff);
                send_message(desc, msg, buff);
                admin_workplace(desc, 2);
                return;
            }
            ++index;
        } 
        if(flag){
            lock = file_write_unlock(lock, fd);
            close(fd);
            admin_train_op(desc, --try_count);
        }
        else{
            write(fd, &train, sizeof(train));
            lock = file_write_unlock(lock, fd);
            close(fd);
            char msg[1024] = "*Train Number ";
            strcat(msg, train.number);
            strcat(msg, " (");
            strcat(msg, train.name);
            strcat(msg, ") is successfully added in the train list.\n");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            admin_workplace(desc, 2);
        }
    }else if(strcmp("2", buff) == 0){
        char message[1024] = "\nOperation Delete a Train";
        int index = 0;
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter Train Number(to be deleted): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(train.number, buff);
        int fd = open("train.dat", O_RDWR);
        lock = file_write_lock(lock, fd);
        while(read(fd, &temp, sizeof(temp)) > 0){
            if(strcmp(train.number, temp.number)==0 && strcmp(temp.status, "ACTIVE")==0 && temp.booked_seats==0){
                strcpy(temp.status, "INACTIVE");
                flag = 1;
                break;
            }
            ++index;
        }
        if(flag){
            lseek(fd, index*sizeof(temp), SEEK_SET);
            write(fd, &temp, sizeof(temp));
            lock = file_write_unlock(lock, fd);
            close(fd);
            char msg[1024] = "*Train Number ";
            strcat(msg, temp.number);
            strcat(msg, " (");
            strcat(msg, temp.name);
            strcat(msg, ") is successfully deleted from the train list.\n");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            admin_workplace(desc, 2);
        }else{
            lock = file_write_unlock(lock, fd);
            close(fd);
            char msg[1024] = "**Train Number ";
            strcat(msg, train.number);
            strcat(msg, " not found in the train list OR Train has Bookings registered to it. Kindly try again.");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            admin_train_op(desc, --try_count);
        }        
    }else if(strcmp("3", buff) == 0){
        char message[1024] = "\nOperation Modify a Train- Update Train Details";
        int index = 0;
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter Train Number(to be modified): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(train.number, buff);
        int fd = open("train.dat", O_RDONLY);
        lock = file_read_lock(lock, fd);
        while(read(fd, &temp, sizeof(temp)) > 0){
            if(strcmp(train.number, temp.number)==0 && strcmp(temp.status, "ACTIVE")==0){
                flag = 1;
                break;
            }
            ++index;
        }
        lock = file_read_unlock(lock, fd);
        close(fd);
            
        if(flag){
            admin_train_modify(desc, 2, temp, index);
            admin_workplace(desc, 2);
        }else{
            char msg[1024] = "**Train Number ";
            strcat(msg, train.number);
            strcat(msg, " not found in the train list. Kindly try again.");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            admin_train_op(desc, --try_count);
        }
        
    }else if(strcmp("4", buff) == 0){
        char message[1024] = "\nOperation Retrieve a Train- Retrieve Train Details";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter Train Number(to be retrieved): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(train.number, buff);
        int fd = open("train.dat", O_RDONLY);
        lock = file_read_lock(lock, fd);
        while(read(fd, &temp, sizeof(temp)) > 0){
            if(strcmp(train.number, temp.number)==0){
                flag = 1;
                break;
            }
        }
        lock = file_read_unlock(lock, fd);
        close(fd);
        
        if(flag){
            char value[100];
            char msg[1024] = "\nTrain Number ";
            strcat(msg, temp.number);
            strcat(msg, " (");
            strcat(msg, temp.name);
            strcat(msg, ") details are:");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            strcpy(msg, temp.number);
            strcat(msg, "\t");
            strcat(msg, temp.name);
            strcat(msg, "\t");
            snprintf(value, sizeof(value), "%d", temp.total_seats);
            strcat(msg, value);
            strcat(msg, "(total seats)\t");
            snprintf(value, sizeof(value), "%d", temp.booked_seats);
            strcat(msg, value);
            strcat(msg, "(booked seats)\t");
            strcat(msg, temp.status);
            strcat(msg, "\n");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);            
            admin_workplace(desc, 2);
        }else{
            char msg[1024] = "\n**Train Number ";
            strcat(msg, train.number);
            strcat(msg, " not found in the train list. Kindly try again.");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            admin_train_op(desc, --try_count);
        }

    }else{
        return ;
    }
}

void admin_user_modify(int desc, int try_count, struct Account temp, int index){
    char buff[1024];
    int flag = 0;
    struct flock lock;
    if(try_count<=0){
        char message[1024] = "\n**Maximum invalid input limit reached. Shutting down.";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        return;
    }
    char message[1024]= "\nAccount Attributes Modification - ";
    strcat(message, temp.username);
    strcat(message, "\nPress 1 to change Account Password\nPress 2 to Modify Account Type\nPress any other key to Exit\nEnter your choice: ");
    send_message(desc, "RW", buff);
    send_message(desc, message, buff);
    if(strcmp("1", buff) == 0){
        strcpy(message, "Enter Account Password(new): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(temp.password, buff);
        strcpy(message, "*Account password changed successfully\n");
        flag = 1;
    }
    else if(strcmp("2", buff) == 0){
        int type;
        strcpy(message, "Enter Account type - 1 for User or 2 for Agent or 3 for Admin: ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        sscanf(buff, "%d", &type);
        if(type<=0 && type>3){
            strcpy(message, "**Invalid Account type, Please try again.");
            send_message(desc, "R", buff);
            send_message(desc, message, buff);
            admin_user_modify(desc, --try_count, temp, index);
        }
        else{
            temp.type = type;
            strcpy(message, "*Account type modified successfully\n");
            flag = 1;
        }
    }else{
        return ;
    }

    if(flag){
        char msg[1024] = "\nUsername ";
        char value[100];
        strcat(msg, temp.username);
        strcat(msg, " (");
        switch (temp.type){
        case 1:
            strcat(msg, "(User) ");
            strcpy(value, "User(account type)\t");
            break;
        case 2:
            strcat(msg, "(Agent) ");
            strcpy(value, "Agent(account type)\t");
            break;
        case 3:
            strcat(msg, "(Admin) ");
            strcpy(value, "Admin(account type)\t");
            break;
        default:
            strcat(msg, "(Not Possible) ");
            strcpy(value, "NA(account type)\t");
            break;
        }
        strcat(msg, "modified details are:");
        send_message(desc, "R", buff);
        send_message(desc, msg, buff);
        strcpy(msg, temp.username);
        strcat(msg, "\t");
        strcat(msg, temp.password);
        strcat(msg, "(password)\t");
        strcat(msg, value);
        strcat(msg, temp.status);
        send_message(desc, "R", buff);
        send_message(desc, msg, buff);            
        int fd = open("login.dat", O_RDWR);
        lock = file_write_lock(lock, fd);
        lseek(fd, index*sizeof(temp), SEEK_SET);
        write(fd, &temp, sizeof(temp));
        lock = file_write_unlock(lock, fd);
        close(fd);
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        admin_workplace(desc, 2);
        return;
    }
}

void delete_bookings_user(struct Account temp){
}

void admin_user_op(int desc, int try_count){
    char buff[1024];
    struct flock lock;
    int index = 0;
    if(try_count<=0){
        char message[1024] = "\n**Maximum invalid input limit reached. Shutting down.";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        return;
    }
    char message[1024]= "\nUser/Agent/Admin Account Operations\nPress 1 to Add an Account\nPress 2 to Delete an Account\nPress 3 to Update an Account\nPress 4 to Retrieve an Account Details\nPress any other key to Exit\nEnter your choice: ";
    struct Account user, temp;
    int flag = 0;
    send_message(desc, "RW", buff);
    send_message(desc, message, buff);
    if(strcmp("1", buff) == 0){
        char message[1024] = "\nOperation Add an Account- Add Account Details";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter Account Name: ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(user.username, buff);
        strcpy(message, "Enter Account Password: ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(user.password, buff);
        strcpy(message, "Enter Account type - 1 for User or 2 for Agent or 3 for Admin: ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        sscanf(buff, "%d", &user.type);
        user.session = 0;
        if(user.type<=0 || user.type>3){
            strcpy(message, "**Invalid Account type, Please try again.");
            send_message(desc, "R", buff);
            send_message(desc, message, buff);
            admin_user_op(desc, --try_count);
        }
        else{
            strcpy(user.status, "ACTIVE");
            int fd = open("login.dat", O_RDWR);
            lock = file_write_lock(lock, fd);
            while(read(fd, &temp, sizeof(temp)) > 0){
                if(strcmp(user.username, temp.username)==0 && strcmp(temp.status, "ACTIVE")==0){
                    char msg[1024] = "**Account ";
                    strcat(msg, user.username);
                    strcat(msg, " is already present. Cannot add another account with same username, Kindly try again.");
                    send_message(desc, "R", buff);
                    send_message(desc, msg, buff);
                    flag = 1;
                    break;
                }
                else if(strcmp(user.username, temp.username)==0 && strcmp(temp.status, "INACTIVE")==0){
                    lseek(fd, index*sizeof(temp), SEEK_SET);
                    write(fd, &user, sizeof(user));
                    lock = file_write_unlock(lock, fd);
                    close(fd);
                    char msg[1024] = "*Account ";
                    strcat(msg, user.username);
                    strcat(msg, "(");
                    switch (user.type)
                    {
                    case 1:
                        strcat(msg, "(User) ");
                        break;
                    case 2:
                        strcat(msg, "(Agent) ");
                        break;
                    case 3:
                        strcat(msg, "(Admin) ");
                        break;
                    default:
                        strcat(msg, "(Not Possible) ");
                        break;
                    }
                    strcat(msg, "is successfully added in the accounts list.\n");
                    send_message(desc, "R", buff);
                    send_message(desc, msg, buff);
                    admin_workplace(desc, 2);
                    return;
                }
                ++index;
            }
            if(flag){
                lock = file_write_unlock(lock, fd);
                close(fd);
                admin_user_op(desc, --try_count);
            }
            else{
                write(fd, &user, sizeof(user));
                lock = file_write_unlock(lock, fd);
                close(fd);
                char msg[1024] = "*Account ";
                strcat(msg, user.username);
                strcat(msg, "(");
                switch (user.type)
                {
                case 1:
                    strcat(msg, "(User) ");
                    break;
                case 2:
                    strcat(msg, "(Agent) ");
                    break;
                case 3:
                    strcat(msg, "(Admin) ");
                    break;
                default:
                    strcat(msg, "(Not Possible) ");
                    break;
                }
                strcat(msg, "is successfully added in the accounts list.\n");
                send_message(desc, "R", buff);
                send_message(desc, msg, buff);
                admin_workplace(desc, 2);
            }
        }        
    }else if(strcmp("2", buff) == 0){
        char message[1024] = "\nOperation Delete an Account";
        int index = 0;
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter username(to be deleted): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(user.username, buff);
        int fd = open("login.dat", O_RDWR);
        lock = file_write_lock(lock, fd);
        int fd2 = open("booking.dat", O_RDONLY);
        struct flock lock2;
        lock2 = file_read_lock(lock2, fd2);
        struct Booking book;
        while(read(fd, &temp, sizeof(temp)) > 0){
            if(strcmp(user.username, temp.username)==0 && strcmp(temp.status, "ACTIVE")==0){
                while(read(fd2, &book, sizeof(book))>0){
                    if(strcmp(user.username, book.Anumber)==0 && strcmp(book.status, "CONFIRMED")==0){
                        flag = 0;
                        break;
                    }
                }
                strcpy(temp.status, "INACTIVE");
                flag = 1;
                break;
            }
            ++index;
        }
        lock2 = file_read_unlock(lock2, fd2);
        close(fd2);                
        if(flag){
            lseek(fd, index*sizeof(temp), SEEK_SET);
            write(fd, &temp, sizeof(temp));
            lock = file_write_unlock(lock, fd);
            close(fd);
            char msg[1024] = "*Account ";
            strcat(msg, temp.username);
            switch (temp.type){
            case 1:
                strcat(msg, "(User) ");
                break;
            case 2:
                strcat(msg, "(Agent) ");
                break;
            case 3:
                strcat(msg, "(Admin) ");
                break;
            default:
                strcat(msg, "(Not Possible) ");
                break;
            }
            strcat(msg, "is successfully deleted from the accounts list.\n");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            admin_workplace(desc, 2);
        }else{
            lock = file_write_unlock(lock, fd);
            close(fd);
            char msg[1024] = "**Account (";
            strcat(msg, user.username);
            strcat(msg, ") not found in the accounts list OR Bookings done for given user account. Kindly try again.");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            admin_user_op(desc, --try_count);
        }        
    }else if(strcmp("3", buff) == 0){
        char message[1024] = "\nOperation Modify an Account- Update Account Details";
        int index = 0;
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter username(to be modified): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(user.username, buff);
        int fd = open("login.dat", O_RDONLY);
        lock = file_read_lock(lock, fd);
        while(read(fd, &temp, sizeof(temp)) > 0){
            if(strcmp(user.username, temp.username)==0 && strcmp(temp.status, "ACTIVE")==0){
                flag = 1;
                break;
            }
            ++index;
        }
        lock = file_read_unlock(lock, fd);
        close(fd);
            
        if(flag){
            admin_user_modify(desc, 2, temp, index);
            admin_workplace(desc, 2);
        }else{
            char msg[1024] = "**Account (";
            strcat(msg, user.username);
            strcat(msg, ") not found in the accounts list. Kindly try again.");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            admin_user_op(desc, --try_count);
        }
        
    }else if(strcmp("4", buff) == 0){
        char message[1024] = "\nOperation Retrieve an Account- Retrieve Account Details";
        send_message(desc, "R", buff);
        send_message(desc, message, buff);
        strcpy(message, "Enter username(to be retrieved): ");
        send_message(desc, "RW", buff);
        send_message(desc, message, buff);
        strcpy(user.username, buff);
        int fd = open("login.dat", O_RDWR);
        lock = file_write_lock(lock, fd);
        while(read(fd, &temp, sizeof(temp)) > 0){
            if(strcmp(user.username, temp.username)==0){
                flag = 1;
                break;
            }
        }
        lock = file_write_unlock(lock, fd);
        close(fd);
        char msg[1024] = "\nUsername ";
        if(flag){
            char value[100];
            strcat(msg, temp.username);
            strcat(msg, " (username)");
            switch (temp.type){
            case 1:
                strcat(msg, "(User) ");
                strcpy(value, "User(account type)\t");
                break;
            case 2:
                strcat(msg, "(Agent) ");
                strcpy(value, "Agent(account type)\t");
                break;
            case 3:
                strcat(msg, "(Admin) ");
                strcpy(value, "Admin(account type)\t");
                break;
            default:
                strcat(msg, "(Not Possible) ");
                strcpy(value, "NA(account type)\t");
                break;
            }
            strcat(msg, "details are:");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            strcpy(msg, temp.username);
            strcat(msg, "\t");
            strcat(msg, temp.password);
            strcat(msg, "(password)\t");
            strcat(msg, value);
            strcat(msg, temp.status);
            strcat(msg, "\n");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);            
            admin_workplace(desc, 2);
        }else{
            char msg[1024] = "**Account (";
            strcat(msg, user.username);
            strcat(msg, ") not found in the accounts list. Kindly try again.");
            send_message(desc, "R", buff);
            send_message(desc, msg, buff);
            admin_user_op(desc, --try_count);
        }

    }else{
        return;
    }
    return;
}

void admin_workplace(int desc, int try_count){
    char input[1024];
    if(try_count<=0){
        char message[1024] = "\n**Maximum invalid input limit reached. Shutting down.";
        send_message(desc, "R", input);
        send_message(desc, message, input);
        return;
    }
    char message[1024] = "Press 1 for Operation on Trains\nPress 2 for Operations on User Accounts\nPress any other key to Exit\nEnter your choice: ";
    send_message(desc, "RW", input);
    send_message(desc, message, input);
    if(strcmp("1", input)==0){
        admin_train_op(desc, 2);
    }
    else if(strcmp("2", input) == 0){
        admin_user_op(desc, 2);
    }
    else{
        return;
    }
}

void verify_credentials(int desc, char *username, char *password){
    struct Account account;
    struct flock lock;
    int fd = open("login.dat", O_RDWR);
    lock = file_write_lock(lock, fd);
    char input[1024];
    int index = 0;
    if(fd == -1){
        char file_not_found[] = "Incorrect UserName or Password. Try Again.\n"; 
        send_message(desc, "R", input);
        send_message(desc, file_not_found, input);
        send_message(desc, "CLOSE", input);
    }
    while(read(fd, &account, sizeof(account)) > 0){
        if( strcmp(account.username, username)==0 && strcmp(account.password, password)==0 && strcmp(account.status, "ACTIVE") == 0){
            if(account.type==1 && account.session==1){
                char welcome_msg[1024] = "\nWelcome ";
                strcat(welcome_msg, account.username);
                strcat(welcome_msg, "\n**You are already logged in. You can't login again.\n");
                send_message(desc, "R", input);
                send_message(desc, welcome_msg, input);
                lock = file_write_unlock(lock, fd);
                close(fd);
                send_message(desc, "CLOSE", input);
                return;
            }
            account.session = 1;
            lseek(fd, index*sizeof(account), SEEK_SET);
            write(fd, &account, sizeof(account));
            lock = file_write_unlock(lock, fd);
            close(fd);
            char welcome_msg[1024] = "\nWelcome ";
            strcat(welcome_msg, account.username);
            send_message(desc, "R", input);
            send_message(desc, welcome_msg, input);
            switch(account.type){
                case 1:
                    client_workplace(desc, 2, account);
                    session_logout(desc, account);
                    return;
                case 2:
                    client_workplace(desc, 2, account);
                    session_logout(desc, account);
                    return;
                case 3:
                    admin_workplace(desc, 2);
                    session_logout(desc, account);
                    return;
            }
            break;
        }
        ++index;
    }
    char user_not_found[] = "Incorrect UserName or Password. Try Again.\n"; 
    send_message(desc, "R", input);
    send_message(desc, user_not_found, input);
    send_message(desc, "CLOSE", input);
}

int get_credentials(int desc){
    char username_msg[1024] = "Enter Username: ";
    char password_msg[1024] = "Enter Password: ";
    char username[1024], password[1024];
    char welcome_msg[1024] = "------------Welcome to Ticketing System------------";
    char login_msg[1024] = "Kindly Enter your credentials";
    char input[1024];
    send_message(desc, "R", input);
    send_message(desc, welcome_msg, input);
    send_message(desc, "R", input);
    send_message(desc, login_msg, input);
    send_message(desc, "RW", input);
    send_message(desc, username_msg, username);
    send_message(desc, "RW", input);
    send_message(desc, password_msg, password);
    verify_credentials(desc, username, password);
    send_message(desc, "CLOSE", input);
    return 0;  
}

void client_connection(int desc){
    char input[1024];
    read(desc, input, sizeof(input));
    get_credentials(desc);
}

int main(int argc, char const *argv[]){
    struct sockaddr_in server, client;
    int socket_desc, size_client, client_desc;
    initial_setup();
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    // server.sin_addr.s_addr = inet_addr("127.0.0.1");

    server.sin_port = htons(PORT);
    if(bind(socket_desc, (void *)(&server), sizeof(server)) < 0) {
	    perror("Error on binding:");
	    exit(EXIT_FAILURE);
	}
    listen(socket_desc, 5);
    while(1){
        size_client = sizeof(client);
	    if((client_desc = accept(socket_desc, (struct sockaddr*)&client, &size_client)) < 0) {
	        printf("Error on accept.\n");
	        exit(EXIT_FAILURE);
	    }
        if(fork() == 0){
            client_connection(client_desc);
            close(client_desc);
            exit(EXIT_SUCCESS);
        }else{
            close(client_desc);
        }
    }
    return 0;
}