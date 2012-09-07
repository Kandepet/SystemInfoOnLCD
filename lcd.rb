#!/usr/bin/env ruby

require 'pp'

free = `free -m`
mem_info =  /Mem:\s+(\d+)\s+(\d+)\s+(\d+)\s+\d+\s+\d+\s+(\d+)/.match(free)
swap_info = /^Swap:\s+(\d+)\s+(\d+)\s+(\d+)/.match(free)

#pp mem_info
#pp swap_info
mem_total = mem_info[1].to_f
mem_used = mem_info[2].to_f
mem_free = mem_info[3].to_f

swap_total = swap_info[1].to_f
swap_used = swap_info[2].to_f
swap_free = swap_info[3].to_f

mem_used_pct = mem_used / mem_total * 100
mem_free_pct = mem_free / mem_total * 100

swap_used_pct = swap_used / swap_total * 100
swap_free_pct = swap_free / swap_total * 100

cpu_util=`mpstat -u`
cpu_util_info = /.+all\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)/.match(cpu_util)

#pp cpu_util_info
cpu_usr_util = cpu_util_info[1].to_f
cpu_nice_util = cpu_util_info[2].to_f
cpu_sys_util = cpu_util_info[3].to_f
cpu_iowait_util = cpu_util_info[4].to_f
cpu_irq_util = cpu_util_info[5].to_f
cpu_soft_util = cpu_util_info[6].to_f
cpu_idle_util = cpu_util_info[9].to_f

cpu_util_pct = cpu_usr_util + cpu_sys_util + cpu_iowait_util + cpu_irq_util + cpu_soft_util

uptime_info=`uptime`
uptime = /\d\d:\d\d:\d\d\sup\s([^,]*)/.match(uptime_info)
load_avg = /average:\s(\d+\.\d\d),\s(\d+\.\d\d),\s(\d+\.\d\d)/.match(uptime_info)

#pp uptime
#pp load_avg

puts "{ "
puts "    \"CPU\" : [\"CPU: #{format("%3.2f", cpu_util_pct)}%\", \"U:#{format("%2d", cpu_usr_util)}% S:#{format("%2d", cpu_sys_util)}% I:#{format("%2d", cpu_idle_util)}%\"],"
puts "    \"MEMORY\" : [\"Mem Used: #{format("%3.2f", mem_used_pct)}%\", \"Mem Free: #{format("%3.2f", mem_free_pct)}%\"],"
puts "    \"SWAP\" : [\"Swap Used: #{format("%3.2f", swap_used_pct)}%\", \"Swap Free: #{format("%3.2f", swap_free_pct)}%\"],"
puts "    \"LOAD\" : [\"Up: #{uptime[1]}\", \"Load: #{load_avg[1]} #{load_avg[2]} #{load_avg[3]}\"]"
puts "}"
