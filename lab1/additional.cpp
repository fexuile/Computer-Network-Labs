TEST(FTPServer, Get) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 3, 1) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(500000);

    /** Generate Content **/
    uint32_t random_name = rand() % 100000000;
    char* random_content = new char[1024 * 1024];
    std::ofstream fout((tmp_dir_ser / (std::to_string(random_name) + ".txt")).string(), std::ios::out);
    int count = 0;
    while(count <= 1024 * 1024) {
        random_content[count] = rand() % 10 + '0';
        fout << random_content[count];
        count++;
    }
    fout.close();
    usleep(500000);
    /** Generate Content **/

    cmd_str = "get " + std::to_string(random_name) + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_cli / (std::to_string(random_name) + ".txt")).string(), std::ios::in);
    char* get_content = new char[1024 * 1024];
    int tmp = 0;
    if(fin){
        while(tmp != count) {
            fin >> get_content[tmp];
            EXPECT_EQ(get_content[tmp], random_content[tmp]);
            tmp++;
        } 
    }else EXPECT_EQ(0, 1);
    // EXPECT_EQ(get_content, random_content);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPServer, Put) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 4, 1) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(500000);

    /** Generate Content **/
    uint32_t random_name = rand() % 100000000;
    char* random_content = new char[1024 * 1024];
    std::ofstream fout((tmp_dir_cli / (std::to_string(random_name) + ".txt")).string(), std::ios::out);
    int count = 0;
    while(count <= 1024 * 1024) {
        random_content[count] = rand() % 10 + '0';
        fout << random_content[count];
        count++;
    }
    fout.close();
    usleep(500000);
    /** Generate Content **/

    cmd_str = "put " + std::to_string(random_name) + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_ser / (std::to_string(random_name) + ".txt")).string(), std::ios::in);
    char* get_content = new char[1024 * 1024];
    int tmp = 0;
    if(fin){
        while(tmp != count) {
            fin >> get_content[tmp];
            EXPECT_EQ(get_content[tmp], random_content[tmp]);
            tmp++;
        } 
    }else EXPECT_EQ(0, 1);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPClient, Get) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 3, 0) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(500000);

    /** Generate Content **/
    uint32_t random_name = rand() % 100000000;
    char* random_content = new char[1024 * 1024];
    std::ofstream fout((tmp_dir_ser / (std::to_string(random_name) + ".txt")).string(), std::ios::out);
    int count = 0;
    while(count <= 1024 * 1024) {
        random_content[count] = rand() % 10 + '0';
        fout << random_content[count];
        count++;
    }
    fout.close();
    usleep(500000);
    /** Generate Content **/

    cmd_str = "get " + std::to_string(random_name) + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_cli / (std::to_string(random_name) + ".txt")).string(), std::ios::in);
    char* get_content = new char[1024 * 1024];
    int tmp = 0;
    if(fin){
        while(tmp != count) {
            fin >> get_content[tmp];
            EXPECT_EQ(get_content[tmp], random_content[tmp]);
            tmp++;
        } 
    }else EXPECT_EQ(0, 1);

    clearProcess(client_pid);
    clearProcess(server_pid);
}


TEST(FTPClient, Put) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 4, 0) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(500000);

    /** Generate Content **/
    uint32_t random_name = rand() % 100000000;
    char* random_content = new char[1024 * 1024];
    std::ofstream fout((tmp_dir_cli / (std::to_string(random_name) + ".txt")).string(), std::ios::out);
    int count = 0;
    while(count <= 1024 * 1024) {
        random_content[count] = rand() % 10 + '0';
        fout << random_content[count];
        count++;
    }
    fout.close();
    usleep(500000);
    /** Generate Content **/

    cmd_str = "put " + std::to_string(random_name) + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_ser / (std::to_string(random_name) + ".txt")).string(), std::ios::in);
    char* get_content = new char[1024 * 1024];
    int tmp = 0;
    if(fin){
        while(tmp != count) {
            fin >> get_content[tmp];
            EXPECT_EQ(get_content[tmp], random_content[tmp]);
            tmp++;
        } 
    }else EXPECT_EQ(0, 1);

    clearProcess(client_pid);
    clearProcess(server_pid);
}
