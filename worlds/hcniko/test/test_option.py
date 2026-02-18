from . import HereComesNikoTestBase


class TestDefault(HereComesNikoTestBase):
    options = {}


class TestHiredGoal(HereComesNikoTestBase):
    options = {
        "GoalCompletion": 0
    }


class TestEmployeeGoal(HereComesNikoTestBase):
    options = {
        "GoalCompletion": 1
    }

