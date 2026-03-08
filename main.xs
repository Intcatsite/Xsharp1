type=comments Игра "Угадай число"

give.me(type_give=module(random))

var(name="secret" module.random int(1)_to_int(100))
var(name="guess" type_var=input("Угадай число от 1 до 100:"))

?(if_type check("guess"="secret"))
    write(write_type=common.text("Поздравляю! Ты угадал!"))
???:
    write(write_type=common.text("Не угадал! Попробуй ещё раз."))
end.block
